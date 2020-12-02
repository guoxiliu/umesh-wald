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

/* no computations at all - just dumps the mesh that's implicit in the
   input umesh, and dumps it in obj format (other prims get
   ignored) */ 

#include "umesh/io/ugrid32.h"
#include "umesh/io/UMesh.h"
#include "umesh/RemeshHelper.h"
#include "umesh/extractSurfaceMesh.h"
#include <algorithm>

namespace umesh {

 extern "C" int main(int ac, char **av)
  {
    try {
      std::string inFileName;
      std::string outFileName;

      for (int i = 1; i < ac; i++) {
        const std::string arg = av[i];
        if (arg == "-o")
          outFileName = av[++i];
        else if (arg[0] != '-')
          inFileName = arg;
        else {
          throw std::runtime_error("./umeshDumpSurfaceMesh <in.umesh> -o <out.obj>");
        }
      }
      
      std::cout << "loading umesh from " << inFileName << std::endl;
      UMesh::SP inMesh = io::loadBinaryUMesh(inFileName);
      if (inMesh->triangles.empty() &&
          inMesh->quads.empty())
        throw std::runtime_error("umesh does not contain any surface elements...");

      UMesh::SP outMesh = extractSurfaceMesh(inMesh);

      std::cout << "extracted surface of " << outMesh->toString() << std::endl;
      std::cout << "... saving (in OBJ format) to " << outFileName << std::endl;
      std::ofstream out(outFileName);
      for (auto vtx : outMesh->vertices)
        out << "v " << vtx.x << " " << vtx.y << " " << vtx.z << std::endl;
      for (auto idx : outMesh->triangles)
        out << "f " << (idx.x+1) << " " << (idx.y+1) << " " << (idx.z+1) << std::endl;
      std::cout << "... done" << std::endl;
    }
    catch (std::exception &e) {
      std::cerr << "fatal error " << e.what() << std::endl;
      exit(1);
    }
    return 0;
  }  
} // ::umesh
