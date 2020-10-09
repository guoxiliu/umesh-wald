// ======================================================================== //
// Copyright 2018-2020 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

/* information on fund3d data format provided by Pat Moran - greatly
   indebted! All loader code rewritten from scratch. */

#include "umesh/io/ugrid32.h"
#include "umesh/RemeshHelper.h"

namespace umesh {

  std::string path = "";
  
  std::string scalarsPath = "";
  
  /*! time step to merge in - gets ignored if no scalarPath is set;
    otherwise indiceaes which of the times steps to use; -1 means
    'last one in file' */
  int timeStep = -1;

  /*! variable to load in */
  std::string variable = "";


  /*! class that loads a fun3d "volume_data" file */
  struct Fun3DScalarsReader {

    Fun3DScalarsReader(const std::string &fileName)
    {
      in = std::ifstream(fileName,std::ios::binary);
      uint32_t magicNumber = io::readElement<uint32_t>(in);

      std::string versionString = io::readString(in);
      std::cout << "Found fun3d file with version string "
                << versionString << std::endl;
      
      uint32_t ignore = io::readElement<uint32_t>(in);
      variableNames.resize(io::readElement<uint32_t>(in));
      for (auto &var : variableNames)
        var = io::readString(in);

      globalVertexIDs.resize(numScalars);
      io::readArray(in,globalVertexIDs.data(),globalVertexIDs.size());

      while (1) {
        uint32_t timeStepID = io::readElement<uint32_t>(in);
        if (in.eof() || !in.good()) continue;
        timeStepOffsets[timeStepID] = in.tellg();

        in.seekg((size_t)in.tellg()
                 +variableNames.size()*globalVertexIDs.size()*sizeof(float)
                 -sizeof(timeStepID),
                 std::ios::beg);
      }
    }

    void readTimeStep(std::vector<float> &scalars,
                      const std::string &desiredVariable,
                      int desiredTimeStep)
    {
      scalars.resize(globalVertexIDs.size());
      
      /* offsets based on _blocks_ of time steps (one per variable) */
      auto it = timeStepOffsets.find(desiredTimeStep);
      if (it == timeStepOffsets.end())
        throw std::runtime_error("could not find requested time step!");
      size_t offset = it->second;

      /* offsets based on which variable */
      for (int i=0;;i++) {
        if (i >= variableNames.size())
          throw std::runtime_error("couldn't find requested variable");
        if (variableNames[i] == desiredVariable)
          break;
        offset += scalars.size()*sizeof(float);
      }
      
      in.seekg(offset);
      io::readArray(in,scalars.data(),scalars.size());
    }
    
    size_t numScalars;
    std::ifstream in;
    std::vector<std::string> variableNames;
    std::vector<size_t>      globalVertexIDs;
    /*! .first is time step ID, .second is the offset in the file */
    std::map<int,size_t>  timeStepOffsets;
    size_t sizeOfTimeStep;
  };
    
  void getInfo(const std::string &scalarsFileName,
               std::vector<std::string> &variables,
               std::vector<int> &timeSteps)
  {
    Fun3DScalarsReader scalarReader(scalarsFileName);
    variables = scalarReader.variableNames;
    timeSteps.clear();
    for (auto it : scalarReader.timeStepOffsets)
      timeSteps.push_back(it.first);
  }
  
  struct MergedMesh {

    MergedMesh() 
      : merged(std::make_shared<UMesh>())
    {}

    void loadScalars(UMesh::SP mesh, int fileID,
                     std::string &fieldName,
                     /*! where each one of the given "volume_data" and
                         "mesh" files' vertices are supposed to go in
                         the global, reconstituted file */
                     std::vector<size_t> &globalVertexIDs)
    {
      if (scalarsPath == "")
        return;
      
      std::string scalarsFileName = scalarsPath // + "volume_data."
        +std::to_string(fileID);
      std::cout << "reading time step " << timeStep
                << " from " << scalarsFileName << std::endl;

      Fun3DScalarsReader reader(scalarsFileName);
      reader.readTimeStep(scalars,fieldName,timeStep);
      globalVertexIDs = reader.globalVertexIDs;
      
    //   std::ifstream file(scalarsFileName,std::ios::binary);
    //   if (!file.good())
    //     throw std::runtime_error("error opening scalars file....");
      
    //   scalars.resize(mesh->vertices.size());
    //   size_t numBytes = sizeof(float)*mesh->vertices.size();

    //   file.seekg(timeStep*numBytes,
    //              timeStep<0
    //              ? std::ios::end
    //              : std::ios::beg);
      
    //   file.read((char*)scalars.data(),numBytes);
    //   if (!file) std::cout << "FILE INVALID" << std::endl;
    //   if (!file.good())
    //     throw std::runtime_error("error reading scalars....");
    //   std::cout << "read " << prettyNumber(scalars.size())
    //             << " scalars (first one is " << scalars[0] << ")" << std::endl;
    }
      
    bool addPart(int fileID)
    {
      std::cout << "----------- part " << fileID << " -----------" << std::endl;
      std::string metaFileName = path + "ta."+std::to_string(fileID);
      std::string meshFileName = path + "sh.lb4."+std::to_string(fileID);
      std::cout << "reading from " << meshFileName << std::endl;
      std::cout << "     ... and " << metaFileName << std::endl;
      struct {
        int tets, pyrs, wedges, hexes;
      } meta;

      {
        FILE *metaFile = fopen(metaFileName.c_str(),"r");
        if (!metaFile) return false;
        assert(metaFile);
        
        int rc =
          fscanf(metaFile,
                 "n_owned_tetrahedra %i\nn_owned_pyramids %i\nn_owned_prisms %i\nn_owned_hexahedra %i\n",
                 &meta.tets,&meta.pyrs,&meta.wedges,&meta.hexes);
        assert(rc == 4);
        fclose(metaFile);
      }

      UMesh::SP mesh = io::UGrid32Loader::load(meshFileName);

      std::string fieldName;
      loadScalars(mesh,fileID,fieldName,globalVertexIDs);

      size_t requiredVertexArraySize = merged->vertices.size();
      for (auto globalID : globalVertexIDs)
        requiredVertexArraySize = std::max(requiredVertexArraySize,size_t(globalID)+1);
      if (!merged->perVertex) {
        merged->perVertex = std::make_shared<Attribute>();
        merged->perVertex->name = variable;
      }
      
      merged->perVertex->values.resize(requiredVertexArraySize);
      merged->vertices.resize(requiredVertexArraySize);
      for (int i=0;i<mesh->vertices.size();i++) {
        merged->vertices[globalVertexIDs[i]] = mesh->vertices[i];
        merged->perVertex->values[globalVertexIDs[i]] = scalars[i];
      }
      // for (auto in : mesh->tets) {
      std::cout << "merging in " << prettyNumber(meta.tets) << " out of " << prettyNumber(mesh->tets.size()) << " tets" << std::endl;
      for (int i=0;i<meta.tets;i++) {
        auto in = mesh->tets[i];
        if (!(i % 100000)) { std::cout << "." << std::flush; };
        UMesh::Tet out;
        translate((uint32_t*)&out,(const uint32_t*)&in,4,mesh->vertices,fileID);
        merged->tets.push_back(out);
      }
      std::cout << std::endl;

      std::cout << "merging in " << prettyNumber(meta.pyrs) << " out of " << prettyNumber(mesh->pyrs.size()) << " pyrs" << std::endl;
      for (int i=0;i<meta.pyrs;i++) {
        auto in = mesh->pyrs[i];
        UMesh::Pyr out;
        translate((uint32_t*)&out,(const uint32_t*)&in,5,mesh->vertices,fileID);
        merged->pyrs.push_back(out);
      }
      
      std::cout << "merging in " << prettyNumber(meta.wedges) << " out of " << prettyNumber(mesh->wedges.size()) << " wedges" << std::endl;
      for (int i=0;i<meta.wedges;i++) {
        auto in = mesh->wedges[i];
        UMesh::Wedge out;
        translate((uint32_t*)&out,(const uint32_t*)&in,6,mesh->vertices,fileID);
        merged->wedges.push_back(out);
      }

      std::cout << "merging in " << prettyNumber(meta.hexes)
                << " out of " << prettyNumber(mesh->hexes.size())
                << " hexes" << std::endl;
      for (int i=0;i<meta.hexes;i++) {
        auto in = mesh->hexes[i];
        UMesh::Hex out;
        translate((uint32_t*)&out,(const uint32_t*)&in,8,mesh->vertices,fileID);
        merged->hexes.push_back(out);
      }
      
      std::cout << "merging in " << prettyNumber(mesh->triangles.size())
                << " triangles" << std::endl;
      for (int i=0;i<mesh->triangles.size();i++) {
        auto in = mesh->triangles[i];
        UMesh::Triangle out;
        translate((uint32_t*)&out,(const uint32_t*)&in,3,mesh->vertices,fileID);
        merged->triangles.push_back(out);
      }
      
      std::cout << "merging in " << prettyNumber(mesh->quads.size())
                << " quads" << std::endl;
      for (int i=0;i<mesh->quads.size();i++) {
        auto in = mesh->quads[i];
        UMesh::Quad out;
        translate((uint32_t*)&out,(const uint32_t*)&in,4,mesh->vertices,fileID);
        merged->quads.push_back(out);
      }
      std::cout << " >>> done part " << fileID << ", got " << merged->toString(false) << " (note it's OK that bounds aren't set yet)" << std::endl;
      return true;
    }
    
    uint32_t translate(uint32_t in,
                       const std::vector<vec3f> &vertices,
                       int fileID)
    {
      return globalVertexIDs[in];
    }
    
    void translate(uint32_t *out,
                   const uint32_t *in,
                   int N,
                   const std::vector<vec3f> &vertices,
                   int fileID)
    {
      for (int i=0;i<N;i++)
        out[i] = translate(in[i],vertices,fileID);
    }
    
    UMesh::SP merged;
    std::vector<size_t> globalVertexIDs;
    /*! desired time step's scalars for current brick, if provided */
    std::vector<float> scalars;
  };

  void usage(const std::string &error = "")
  {
    if (error != "")
      std::cout << "Fatal error: " << error << std::endl << std::endl;
    std::cout << "./bigLanderMergeMeshes <path> <args>" << std::endl;
    std::cout << "w/ Args: " << std::endl;
    std::cout << "-o <out.umesh>\n\tfilename for output (merged) umesh" << std::endl;
    std::cout << "-n <numFiles> --first <firstFile>\n\t(optional) which range of files to process\n\te.g., --first 2 -n 3 will process files name.2, name.3, and name.4" << std::endl;
    std::cout << "--scalars scalarBasePath\n\twill read scalars from *_volume.X files at given <scalarBasePath>_volume.X" << std::endl;
    std::cout << "-ts <timeStep>" << std::endl;
    std::cout << "-var|--variable <variableName>" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "./umeshImportLanderFun3D /space/fun3d/small/dAgpu0145_Fa_ --scalars /space/fun3d/small/10000unsteadyiters/dAgpu0145_Fa_volume_data. -o /space/fun3d/merged_lander_small.umesh" << std::endl;
    std::cout << "to print available variables and time steps, call with file names but do not specify time step or variables" << std::endl;
    exit( error != "");
  }
  
  extern "C" int main(int ac, char **av)
  {
    int begin = 1;
    int num = 10000;

    std::string outFileName = "";//"huge-lander.umesh";
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      if (arg == "-n" || arg == "--num" || arg == "-num")
        num = atoi(av[++i]);
      else if (arg == "--first")
        begin = atoi(av[++i]);
      else if (arg == "-s" || arg == "--scalars")
        scalarsPath = av[++i];
      else if (arg == "-ts" || arg == "--time-step")
        timeStep = atoi(av[++i]);
      else if (arg == "-var" || arg == "--variable")
        variable = av[++i];
      else if (arg == "-o")
        outFileName = av[++i];
      else if (arg[0] != '-')
        path = arg;
      else
        usage("unknown cmdline arg "+arg);
    }
    if (path == "") usage("no input path specified");
    if (outFileName == "") usage("no output filename specified");

    if (timeStep < 0 || variable == "") {
      std::string firstFileName = scalarsPath+std::to_string(begin);
      std::vector<std::string> variables;
      std::vector<int> timeSteps;
      getInfo(firstFileName,variables,timeSteps);
      std::cout << "File Info: " << std::endl;
      std::cout << "variables:";
      for (auto var : variables) std::cout << " " << var;
      std::cout << std::endl;
      std::cout << "timeSteps:";
      for (auto var : timeSteps) std::cout << " " << var;
      std::cout << std::endl;
      exit(0);
    }
    
    MergedMesh mesh;
    
    for (int i=begin;i<(begin+num);i++)
      if (!mesh.addPart(i))
        break;

    mesh.merged->finalize();
    
    std::cout << "done all parts, saving output to "
              << outFileName << std::endl;
    mesh.merged->saveTo(outFileName);
    std::cout << "done all ..." << std::endl;
  }
  
} // ::umesh
