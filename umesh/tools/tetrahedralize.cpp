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
#include "umesh/io/UMesh.h"
#include "umesh/tetrahedralize.h"

namespace umesh {

  
  extern "C" int main(int ac, char **av)
  {
    std::string inFileName;
    std::string outFileName;
    /*! if enabled, we'll only save the tets that _we_ created, not
        those that were in the file initially */
    bool skipActualTets = false;
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      PRINT(arg);
      if (arg == "-o")
        outFileName = av[++i];
      else if (arg == "--skip-actual-tets")
        skipActualTets = true;
      else if (arg[0] != '-')
        inFileName = arg;
      else
        throw std::runtime_error
          ("./umeshTetrahedralize <in.umesh> -o <out.umesh> [--skip-actual-tets]");
    }

    std::cout << "loading umesh from " << inFileName << std::endl;
    UMesh::SP in = io::loadBinaryUMesh(inFileName);
    
    PRINT(prettyNumber(in->tets.size()));
    PRINT(prettyNumber(in->pyrs.size()));
    PRINT(prettyNumber(in->wedges.size()));
    PRINT(prettyNumber(in->hexes.size()));
    if (in->pyrs.empty() &&
        in->wedges.empty() &&
        in->hexes.empty()) {
      std::cout << OWL_TERMINAL_RED << std::endl;
      std::cout << "*******************************************************" << std::endl;
      std::cout << "WARNING: umesh already contains only tets..." << std::endl;
      std::cout << "*******************************************************" << std::endl;
      std::cout << OWL_TERMINAL_DEFAULT << std::endl;
    }
    
    UMesh::SP out = tetrahedralize(in);
    std::cout << "done all prims, saving output to " << outFileName << std::endl;
    PRINT(prettyNumber(out->tets.size()));
    PRINT(prettyNumber(out->pyrs.size()));
    PRINT(prettyNumber(out->wedges.size()));
    PRINT(prettyNumber(out->hexes.size()));
    
    io::saveBinaryUMesh(outFileName,out);
    std::cout << "done all ..." << std::endl;
    
  }
} // ::umesh
