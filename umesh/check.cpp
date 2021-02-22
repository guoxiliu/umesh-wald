// ======================================================================== //
// Copyright 2018-2021 Ingo Wald                                            //
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

#include "umesh/check.h"

namespace umesh {

#ifndef PRINT
#ifdef __CUDA_ARCH__
# define PRINT(va) /**/
# define PING /**/
#else
# define PRINT(var) std::cout << #var << "=" << var << std::endl;
#ifdef __WIN32__
# define PING std::cout << __FILE__ << "::" << __LINE__ << ": " << __FUNCTION__ << std::endl;
#else
# define PING std::cout << __FILE__ << "::" << __LINE__ << ": " << __PRETTY_FUNCTION__ << std::endl;
#endif
#endif
#endif

  
  /*! perform some sanity checking of the given mesh (checking indices
    are valid, etc) */
  void sanityCheck(UMesh::SP mesh)
  {
    if (!mesh) throw std::runtime_error("#check: null umesh");
    if (mesh->numVolumeElements() == 0)
      std::cout <<"#check - WARNING: "
                << "num volume elemnts in mesh is 0!?" << std::endl;
    if (mesh->perVertex && mesh->perVertex->values.size() != mesh->vertices.size())
      throw std::runtime_error("attribute size doesn't match vertex array size");
    
    for (auto p : mesh->tets) {
      for (int i=0;i<p.numVertices;i++) {
        if (p[i] < 0) {
          PRINT(mesh->vertices.size());
          PRINT(p);
          throw std::runtime_error("#check: mesh has negative index!?");
        }
        if (p[i] >= mesh->vertices.size()) {
          PRINT(mesh->vertices.size());
          PRINT(p);
          throw std::runtime_error("#check: mesh has index greater than vertex array size!?");
        }
      }
    }

    for (auto p : mesh->pyrs) {
      for (int i=0;i<p.numVertices;i++) {
        if (p[i] < 0) {
          PRINT(mesh->vertices.size());
          PRINT(p);
          throw std::runtime_error("#check: mesh has negative index!?");
        }
        if (p[i] >= mesh->vertices.size()) {
          PRINT(mesh->vertices.size());
          PRINT(p);
           throw std::runtime_error("#check: mesh has index greater than vertex array size!?");
        }
      }
    }

    for (auto p : mesh->wedges) {
      for (int i=0;i<p.numVertices;i++) {
        if (p[i] < 0) {
          PRINT(mesh->vertices.size());
          PRINT(p);
          throw std::runtime_error("#check: mesh has negative index!?");
        }
        if (p[i] >= mesh->vertices.size())  {
          PRINT(mesh->vertices.size());
          PRINT(p);
          throw std::runtime_error("#check: mesh has index greater than vertex array size!?");
        }
      }
    }

    for (auto p : mesh->hexes) {
      for (int i=0;i<p.numVertices;i++) {
        if (p[i] < 0)  {
          PRINT(mesh->vertices.size());
          PRINT(p);
          throw std::runtime_error("#check: mesh has negative index!?");
        }
        if (p[i] >= mesh->vertices.size()) {
          PRINT(mesh->vertices.size());
          PRINT(p);
           throw std::runtime_error("#check: mesh has index greater than vertex array size!?");
        }
      }
    }

    for (auto p : mesh->triangles) {
      for (int i=0;i<p.numVertices;i++) {
        if (p[i] < 0)  {
          PRINT(mesh->vertices.size());
          PRINT(p);
          throw std::runtime_error("#check: mesh has negative index!?");
        }
        if (p[i] >= mesh->vertices.size()) {
          PRINT(mesh->vertices.size());
          PRINT(p);
           throw std::runtime_error("#check: mesh has index greater than vertex array size!?");
        }
      }
    }
      
    for (auto p : mesh->quads) {
      for (int i=0;i<p.numVertices;i++) {
        if (p[i] < 0) {
          PRINT(mesh->vertices.size());
          PRINT(p);
           throw std::runtime_error("#check: mesh has negative index!?");
        }
        if (p[i] >= mesh->vertices.size()) {
          PRINT(mesh->vertices.size());
          PRINT(p);
           throw std::runtime_error("#check: mesh has index greater than vertex array size!?");
        }
      }
    }
  }
  
} // :: umesh
