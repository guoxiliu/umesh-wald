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

/* fun3d header reader code provided by Pat Moran (NASA). Greatly
   indebted! */

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

  std::string variable = "";


  /// Provide a container for native volume header data (for a single subdomain).
  struct Header {
    Header() : n_nodes_0(0), n_nodes(0), size(0) {}

    /// File format version.
    std::string version;
    /// FUN3D internal: number of grid points local to the partition.
    size_t n_nodes_0;
    /// Number of nodes (also known as vertices or grid points).
    size_t n_nodes;
    /// Names of variables, in the order they occur in the data.
    std::vector<std::string> variables;
    /// Map from local to global vertex numbering, 1-based.
    std::vector<long> local_to_global;
    /// Header size, in bytes.
    size_t size;
  };


  /*! Read string from \p is, stored as length n followed by n characters. */
  std::string read_string(std::istream& is)
  {
    int32_t length;
    is.read((char*) &length, sizeof(int32_t));
    std::vector<char> chars(length);
    is.read(chars.data(), chars.size());
    return std::string(chars.data(), chars.size());
  }


  /*! Read beginning of native volume file from \p is, write into \p header. */
  void read_native_volume_header(std::istream& is, Header* header)
  {
    int32_t magic;
    is.read((char*) &magic, sizeof(int32_t));
    if (magic != 305419896)
      throw std::runtime_error("bad magic");
    header->version = read_string(is);
    int32_t i32;
    is.read((char*) &i32, sizeof(int32_t));
    header->n_nodes_0 = (size_t) i32;
    is.read((char*) &i32, sizeof(int32_t));
    header->n_nodes = (size_t) i32;
    is.read((char*) &i32, sizeof(int32_t));
    size_t n_variables = (size_t) i32;
    header->variables.resize(n_variables);
    for (size_t i = 0; i < n_variables; ++i)
      header->variables[i] = read_string(is);
    header->local_to_global.resize(header->n_nodes);
    is.read((char*) header->local_to_global.data(),
            header->local_to_global.size() * sizeof(long));
    header->size = is.tellg();
  }


  /*! Read data into \p header, given native volume file \p path. */
  void read_native_volume_header(const std::string& path, Header* header)
  {
    try {
      std::ifstream is(path);
      if (!is)
        throw std::runtime_error("failed on open");
      is.exceptions(std::ios::badbit | std::ios::failbit);
      return read_native_volume_header(is, header);
    }
    catch (std::exception& e) {
      throw std::runtime_error("\"" + path + "\": " + e.what());
    }
  }


  /*! Return index of \p name in \p names. */
  size_t find_variable(const std::string& name, const std::vector<std::string>& names)
  {
    auto it = find(names.begin(), names.end(), name);
    if (it == names.end()) {
      std::string err = "variable " + name + " not in (";
      for (size_t i = 0; i < names.size(); i++) {
        if (i > 0)
          err += ", ";
        err += names[i];
      }
      err += ")";
      throw std::runtime_error(err);
    }
    return distance(names.begin(), it);
  }

  std::vector<int> availableTimeSteps(std::istream& is, const Header& h)
  {
    std::vector<int> result;
    size_t size1 = sizeof(int32_t) +
      h.n_nodes * h.variables.size() * sizeof(float);
    size_t i = 0;

    for ( ; ; i++) {
      is.seekg(h.size + i * size1);
      int32_t tsi;
      is.read((char*) &tsi, sizeof(int32_t));
      if (is.eof())
        return result;
      result.push_back(tsi);
    }
  }

  void getInfo(const std::string &scalarsFileName,
               std::vector<std::string> &variables,
               std::vector<int> &timeSteps)
  {
    std::ifstream file(scalarsFileName,std::ios::binary);
    Header header;
    read_native_volume_header(file, &header);
    variables = header.variables;
    timeSteps = availableTimeSteps(file, header);
  }
  
  /*!
   * Find time step \p ts and return the corresponding index i for the
   * ith time step (index starting at 0).  Throw an exception if not
   * found. If found, \p is will be positioned immediately following the
   * time step integer matching \p ts.
   */
  size_t seek_time_step(std::istream& is, const Header& h, int ts)
  {
    // Disable ios::failbit so end-of-file state does not trigger exception;
    // we test for eof and provide a more meaningful error message.
    std::ios::iostate exceptions = is.exceptions();
    is.exceptions(exceptions & ~std::ios::failbit);

    int ts0 = -1;
    size_t size1 = sizeof(int32_t) +
      h.n_nodes * h.variables.size() * sizeof(float);
    size_t i = 0;
    PRINT(ts0);
    PRINT(size1);
    for ( ; ; i++) {
      is.seekg(h.size + i * size1);
      int32_t tsi;
      is.read((char*) &tsi, sizeof(int32_t));
      PRINT(tsi);
      if (is.eof())
        throw std::runtime_error("time step not found: " + std::to_string(ts));
      if (tsi == ts)
        break;
      if (i == 0) {
        ts0 = tsi;
      }
      else if (i == 1) {
        //
        // Assuming snapshots saved at a fixed stride, calculate where
        // we would expect to find time step ts, based on ts0 and ts1,
        // and see if that is correct.  Seeking to and reading 4-byte
        // tsi values one at a time can be slow going if there are
        // many time steps saved to a file.  Use this heuristic to try
        // to find the matching time step faster.
        //
        int ts1 = tsi;
        int dt = ts1 - ts0;
        if (dt > 0 && (ts - ts0) > 0 && (ts - ts0) % dt == 0) {
          size_t ii = (size_t) ((ts - ts0) / dt);
          is.seekg(h.size + ii * size1);
          is.read((char*) &tsi, sizeof(int32_t));
          if (!is.eof() && tsi == ts) {
            i = ii;
            break;
          }
        }
      }
    }
    is.exceptions(exceptions);
    return i;
  }


  /*!
   * Open native volume file specified by \p path, find \p time_step and
   * variable with given \p name, copy field values to \p data.
   */
  void read_fun3d_native_volume_field(const std::string& path, int time_step,
                                      const std::string& name, float* data)
  {
    try {
      std::ifstream is(path,std::ios::binary);
      if (!is)
        throw std::runtime_error("failed on open");
      is.exceptions(std::ios::badbit | std::ios::failbit);
      Header header;
      read_native_volume_header(is, &header);
      size_t index = find_variable(name, header.variables);
      seek_time_step(is, header, time_step);
      std::vector<float> tmp(header.n_nodes * header.variables.size());
      is.read((char*) tmp.data(), tmp.size() * sizeof(float));
      for (size_t i = 0; i < header.n_nodes; ++i) {
        data[i] = tmp[i * header.variables.size() + index];
      }
    }
    catch (std::exception& e) {
      throw std::runtime_error("\"" + path + "\": " + e.what());
    }
  }


  struct MergedMesh {

    MergedMesh() 
      : merged(std::make_shared<UMesh>()),
        indexer(*merged)
    {
    }

    void loadScalars(UMesh::SP mesh, int fileID,
                     std::string &fieldName)
    {
      if (scalarsPath == "")
        return;
      
      std::string scalarsFileName = scalarsPath // + "volume_data."
        +std::to_string(fileID);
      std::cout << "reading time step " << timeStep
                << " from " << scalarsFileName << std::endl;

#if 1
      Header header;
      read_native_volume_header(scalarsFileName, &header);

      fieldName = variable;
      PRINT(fieldName);
      int time_step = timeStep;
      
      std::vector<float> &data = scalars;
      data.resize(header.n_nodes);
      read_fun3d_native_volume_field(scalarsFileName,//path,
                                     time_step, fieldName, data.data());
      
#else      
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
#endif
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

      UMesh::SP mesh = io::UGrid32Loader::load(meshFileName);

      std::string fieldName;
      loadScalars(mesh,fileID,fieldName);


      
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
      
      std::cout << " >>> done part " << fileID << ", got " << merged->toString(false) << std::endl;
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
