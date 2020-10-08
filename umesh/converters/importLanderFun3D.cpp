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

#include "umesh/io/ugrid32.h"
#include "umesh/RemeshHelper.h"

namespace umesh {

  // const std::string path = "/space/data/huge-still-downloading";
  std::string path = "";// "/space/test/114B_";
  
  // path to *volume.X files from which to merge in a time step (may
  // be "")
  std::string scalarsPath = "";
  
  /*! time step to merge in - gets ignored if no scalarPath is set;
    otherwise indiceaes which of the times steps to use; -1 means
    'last one in file' */
  int timeStep = -1;
    
  
  struct MergedMesh {

    MergedMesh() 
      : merged(std::make_shared<UMesh>()),
        indexer(*merged)
    {
    }

    void loadScalars(UMesh::SP mesh, int fileID)
    {
      if (scalarsPath == "")
        return;
      
      std::string scalarsFileName = scalarsPath // + "volume_data."
        +std::to_string(fileID);
      std::cout << "reading time step " << timeStep
                << " from " << scalarsFileName << std::endl;
      std::ifstream file(scalarsFileName,std::ios::binary);
      if (!file.good())
        throw std::runtime_error("error opening scalars file....");
      
      scalars.resize(mesh->vertices.size());
      size_t numBytes = sizeof(float)*mesh->vertices.size();

      file.seekg(timeStep*numBytes,
                 timeStep<0
                 ? std::ios::end
                 : std::ios::beg);
      
      file.read((char*)scalars.data(),numBytes);
      if (!file) std::cout << "FILE INVALID" << std::endl;
      if (!file.good())
        throw std::runtime_error("error reading scalars....");
      std::cout << "read " << prettyNumber(scalars.size())
                << " scalars (first one is " << scalars[0] << ")" << std::endl;
    }
      
      bool addPart(int fileID)
    {
      std::cout << "----------- part " << fileID << " -----------" << std::endl;
      std::string metaFileName = path + "ta."+std::to_string(fileID);
      std::string meshFileName = path + "sh.lb4."+std::to_string(fileID);
      // std::string metaFileName = path + "meta."+std::to_string(fileID);
      // std::string meshFileName = path + "mesh.lb4."+std::to_string(fileID);
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

      PRINT(prettyNumber(meta.tets));
      PRINT(prettyNumber(meta.pyrs));
      PRINT(prettyNumber(meta.wedges));
      PRINT(prettyNumber(meta.hexes));

      UMesh::SP mesh = io::UGrid32Loader::load(meshFileName);

      loadScalars(mesh,fileID);


      
      // for (auto in : mesh->tets) {
      std::cout << "merging in " << prettyNumber(meta.tets) << " out of " << prettyNumber(mesh->tets.size()) << " tets" << std::endl;
      for (int i=0;i<meta.tets;i++) {
        auto in = mesh->tets[i];
        if (!(i % 100000)) { std::cout << "." << std::flush; };//PRINT(i); PRINT(in); }
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
      
      std::cout << " >>> done part " << fileID << std::endl;
      PRINT(prettyNumber(merged->vertices.size()));
      PRINT(prettyNumber(merged->tets.size()));
      PRINT(prettyNumber(merged->pyrs.size()));
      PRINT(prettyNumber(merged->wedges.size()));
      PRINT(prettyNumber(merged->hexes.size()));
      return true;
    }
    
    uint32_t translate(uint32_t in,
                       const std::vector<vec3f> &vertices,
                       int fileID)
    {
      if (scalars.empty()) {
        size_t tag = (size_t(fileID) << 32) | in;
        return indexer.getID(vertices[in],tag);
      }
      else
        return indexer.getID(vertices[in],scalars[in]);
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
    RemeshHelper indexer;
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
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "./umeshImportLanderFun3D /space/fun3d/small/dAgpu0145_Fa_ --scalars /space/fun3d/small/10000unsteadyiters/dAgpu0145_Fa_volume_data. -o /space/fun3d/merged_lander_small.umesh" << std::endl;

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
      else if (arg == "-o")
        outFileName = av[++i];
      else if (arg[0] != '-')
        path = arg;
      else
        usage("unknown cmdline arg "+arg);
    }

    MergedMesh mesh;
    if (path == "") usage("no input path specified");
    if (outFileName == "") usage("no output filename specified");
    
    for (int i=begin;i<(begin+num);i++)
      if (!mesh.addPart(i))
        break;
    
    std::cout << "done all parts, saving output to "
              << outFileName << std::endl;
    mesh.merged->saveTo(outFileName);
    std::cout << "done all ..." << std::endl;
  }
  
} // ::umesh
