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

#include "RemeshHelper.h"

namespace umesh {

  RemeshHelper::RemeshHelper(UMesh &target)
    : target(target)
  {}

  /*! given a vertex v, return its ID in the target mesh's vertex
    array (if present), or add it (if not). To afterwards allow the
    using libnray to look up which of the inptu vertices ended up
    where we also allow to specify a vertex "tag" for each input
    vertices. meshes should only ever get built with *either* this
    functoin of the one that uses a float scalar, not mixed */
  uint32_t RemeshHelper::getID(const vec3f &v, size_t tag)
  {
    auto it = knownVertices.find(v);
    if (it != knownVertices.end()) {
      return it->second;
    }
    int ID = target.vertices.size();
    knownVertices[v] = ID;
    target.vertexTag.push_back(tag);
    target.vertices.push_back(v);
    return ID;
  }

  /*! given a vertex v and associated per-vertex scalar value s,
    return its ID in the target mesh's vertex array (if present), or
    add it (if not). To afterwards allow the using libnray to look
    up which of the inptu vertices ended up where we also allow to
    specify a vertex "tag" for each input vertices.  meshes should
    only ever get built with *either* this functoin of the one that
    uses a size_t tag, not mixed */
  uint32_t RemeshHelper::getID(const vec3f &v, float scalar)
  {
    auto it = knownVertices.find(v);
    if (it != knownVertices.end()) {
      return it->second;
    }
    int ID = target.vertices.size();
    knownVertices[v] = ID;
    if (!target.perVertex)
      target.perVertex = std::make_shared<Attribute>();
    target.perVertex->values.push_back(scalar);
    target.vertices.push_back(v);
    return ID;
  }

  /*! given a vertex ID in another mesh, return an ID for the
   *output* mesh that corresponds to this vertex (add to output
   if not already present) */
  uint32_t RemeshHelper::translate(const uint32_t in,
                                   UMesh::SP otherMesh)
  {
    if (otherMesh->perVertex) {
      return getID(otherMesh->vertices[in],
                   otherMesh->perVertex->values[in]);
    } else if (!otherMesh->vertexTag.empty()) {
      return getID(otherMesh->vertices[in],
                   otherMesh->vertexTag[in]);
    } else 
      throw std::runtime_error("can't translate a vertex from another mesh that has neither scalars not vertex tags");
  }
    
  void RemeshHelper::translate(uint32_t *indices, int N,
                               UMesh::SP otherMesh)
  {
    for (int i=0;i<N;i++)
      indices[i] = translate(indices[i],otherMesh);
  }

  void RemeshHelper::add(UMesh::SP otherMesh, UMesh::PrimRef primRef)
  {
    switch (primRef.type) {
	       case UMesh::TRI: {
      auto prim = otherMesh->triangles[primRef.ID];
      translate((uint32_t*)&prim,3,otherMesh);
      target.triangles.push_back(prim);
    } break;
    case UMesh::QUAD: {
      auto prim = otherMesh->quads[primRef.ID];
      translate((uint32_t*)&prim,4,otherMesh);
      target.quads.push_back(prim);
    } break;

    case UMesh::TET: {
      auto prim = otherMesh->tets[primRef.ID];
      translate((uint32_t*)&prim,4,otherMesh);
      target.tets.push_back(prim);
    } break;
    case UMesh::PYR: {
      auto prim = otherMesh->pyrs[primRef.ID];
      translate((uint32_t*)&prim,5,otherMesh);
      target.pyrs.push_back(prim);
    } break;
    case UMesh::WEDGE: {
      auto prim = otherMesh->wedges[primRef.ID];
      translate((uint32_t*)&prim,6,otherMesh);
      target.wedges.push_back(prim);
    } break;
    case UMesh::HEX: {
      auto prim = otherMesh->hexes[primRef.ID];
      translate((uint32_t*)&prim,8,otherMesh);
      target.hexes.push_back(prim);
    } break;
    default:
      throw std::runtime_error("un-implemented prim type?");
    }
  }
  
} // ::tetty
