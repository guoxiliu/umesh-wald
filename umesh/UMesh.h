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

#pragma once

#include "owl/common/math/vec.h"
#include "owl/common/math/box.h"
#include "owl/common/parallel/parallel_for.h"
#include <mutex>
#include <vector>
#include <map>

namespace umesh {
  using namespace owl;
  using namespace owl::common;

  typedef owl::interval<float> range1f;
  
  struct Attribute {
    typedef std::shared_ptr<Attribute> SP;

    Attribute(int num=0) : value(num) {}
    
    /*! tells this attribute that its values are set, and precomputations can be done */
    void finalize();
    
    std::string name;
    /*! for now lets implement only float attributes. node/ele files
      actually seem to have ints in the ele file (if they have
      anything at all), but these should still fir into floats, so
      we should be good for now. This may change once we better
      understand where these values come from, and what they
      mean. */
    std::vector<float> value;
    range1f valueRange;
  };

  typedef vec4i Tet;
    
  struct Wedge {
    inline Wedge() {}
    inline Wedge(int v0, int v1, int v2, int v3, int v4, int v5)
      : front(v0,v1,v2),
        back(v3,v4,v5)
    {}
    /*! array operator, assuming VTK ordering (see windingOrder.png
      file for illustration) */
    inline const int &operator[](int i) const {return ((int*)this)[i]; }
    inline int &operator[](int i){return ((int*)this)[i]; }
    vec3i front, back;
  };

  struct Hex {
    inline Hex() {}
    inline Hex(int v0, int v1, int v2, int v3, int v4, int v5, int v6, int v7)
      : base(v0,v1,v2,v3),
        top(v4,v5,v6,v7)
    {}
    /*! array operator, assuming VTK ordering (see windingOrder.png
      file for illustration) */
    inline const int &operator[](int i) const {return ((int*)this)[i]; }
    inline int &operator[](int i){return ((int*)this)[i]; }
    vec4i base;
    vec4i top;
  };
    
  struct Pyr {
    inline Pyr() {}
    inline Pyr(int v0, int v1, int v2, int v3, int v4)
      : base(v0,v1,v2,v3),top(v4)
    {}
      
    /*! array operator, assuming VTK ordering (see windingOrder.png
      file for illustration) */
    inline const int &operator[](int i) const {return ((int*)this)[i]; }
    inline int &operator[](int i){return ((int*)this)[i]; }
    vec4i base;
    int top;
  };
    
  /*! basic unstructured mesh class - one set of 3-float vertices, and
      one std::vector each for tets, wedges, pyramids, and hexes, all
      using VTK format for the vertex index ordering (see
      windingorder.jpg) */
  struct UMesh {
    using Wedge = umesh::Wedge;
    using Hex = umesh::Hex;
    using Tet = umesh::Tet;
    using Pyr = umesh::Pyr;
    
    typedef std::shared_ptr<UMesh> SP;

    typedef enum { TRI, QUAD, TET, PYR, WEDGE, HEX, INVALID } PrimType;
    
    struct PrimRef {
      inline PrimRef() {}
      inline PrimRef(PrimType type, size_t ID) : type(type), ID(ID) {}
      inline PrimRef(const PrimRef &) = default;
      inline bool isTet() const { return type == TET; }
      union {
        struct {
          size_t type:4;
          size_t ID:60;
        };
        size_t as_size_t;
      };
    };
    
    inline void print()
    {
      std::cout << "#verts  : " << prettyNumber(vertex.size()) << std::endl;
      std::cout << "#tris  : " << prettyNumber(triangles.size()) << std::endl;
      std::cout << "#quads : " << prettyNumber(quads.size()) << std::endl;
      std::cout << "#tets  : " << prettyNumber(tets.size()) << std::endl;
      std::cout << "#pyrs  : " << prettyNumber(pyrs.size()) << std::endl;
      std::cout << "#wedges: " << prettyNumber(wedges.size()) << std::endl;
      std::cout << "#hexes : " << prettyNumber(hexes.size()) << std::endl;
    }

    /*! write - binary - to given file */
    void saveTo(const std::string &fileName) const;
    /*! write - binary - to given (bianry) stream */
    void writeTo(std::ostream &out) const;
    
    
    /*! read from given file, assuming file format as used by saveTo() */
    static UMesh::SP loadFrom(const std::string &fileName);
    /*! read from given (binary) stream */
    void readFrom(std::istream &in);
    
    
    /*! create std::vector of primitmive references (bounding box plus
        tag) for every volumetric prim in this mesh */
    void createVolumePrimRefs(std::vector<PrimRef> &result);
    
    inline void createSurfacePrimRefs(std::vector<PrimRef> &result)
    {
      throw std::runtime_error("not implemented..");
    }

    inline size_t size() const
    {
      return
        triangles.size()+
        quads.size()+
        hexes.size()+
        tets.size()+
        wedges.size()+
        pyrs.size();
    }

    inline range1f getValueRange() const {
      // if (perVertex) {
      if (perVertex)
        return perVertex->valueRange;
      // } else {
      //   range1f result;
      //   if (perTet)   result.extend(perTet->valueRange);
      //   if (perHex)   result.extend(perHex->valueRange);
      // }
      throw std::runtime_error("cannot get value range for umesh: no attributes!");
    }
    
    inline box3f getBounds() const
    {
      return bounds;
    }

    inline box4f getBounds4f() const
    {
      return box4f(vec4f(bounds.lower,perVertex->valueRange.lower),
                   vec4f(bounds.upper,perVertex->valueRange.upper));
    }
    



    inline range1f getTetValueRange(const size_t ID) const
    {
      const range1f b = range1f()
        .including(perVertex->value[tets[ID].x])
        .including(perVertex->value[tets[ID].y])
        .including(perVertex->value[tets[ID].z])
        .including(perVertex->value[tets[ID].w]);
      return b;
    }
    
    inline range1f getPyrValueRange(const size_t ID) const
    {
      return range1f()
        .including(perVertex->value[pyrs[ID].top])
        .including(perVertex->value[pyrs[ID].base.x])
        .including(perVertex->value[pyrs[ID].base.y])
        .including(perVertex->value[pyrs[ID].base.z])
        .including(perVertex->value[pyrs[ID].base.w]);
    }
    
    inline range1f getWedgeValueRange(const size_t ID) const
    {
      return range1f()
        .including(perVertex->value[wedges[ID].front.x])
        .including(perVertex->value[wedges[ID].front.y])
        .including(perVertex->value[wedges[ID].front.z])
        .including(perVertex->value[wedges[ID].back.x])
        .including(perVertex->value[wedges[ID].back.y])
        .including(perVertex->value[wedges[ID].back.z])
        ;
    }
    
    inline range1f getHexValueRange(const size_t ID) const
    {
      const range1f b = range1f()
        .including(perVertex->value[hexes[ID].top.x])
        .including(perVertex->value[hexes[ID].top.y])
        .including(perVertex->value[hexes[ID].top.z])
        .including(perVertex->value[hexes[ID].top.w])
        .including(perVertex->value[hexes[ID].base.x])
        .including(perVertex->value[hexes[ID].base.y])
        .including(perVertex->value[hexes[ID].base.z])
        .including(perVertex->value[hexes[ID].base.w]);
      return b;
    }
    
    inline range1f getValueRange(const PrimRef &pr) const
    {
      switch(pr.type) {
      case TET:  return getTetValueRange(pr.ID);
      case PYR:  return getPyrValueRange(pr.ID);
      case WEDGE: return getWedgeValueRange(pr.ID);
      case HEX:  return getHexValueRange(pr.ID);
      default: 
        throw std::runtime_error("not implemented");
      };
    }



    
    

    inline box3f getTetBounds(const size_t ID) const
    {
      const box3f b = box3f()
        .including(vertex[tets[ID].x])
        .including(vertex[tets[ID].y])
        .including(vertex[tets[ID].z])
        .including(vertex[tets[ID].w]);
      return b;
    }
    
    inline box3f getPyrBounds(const size_t ID) const
    {
      return box3f()
        .including(vertex[pyrs[ID].top])
        .including(vertex[pyrs[ID].base.x])
        .including(vertex[pyrs[ID].base.y])
        .including(vertex[pyrs[ID].base.z])
        .including(vertex[pyrs[ID].base.w]);
    }
    
    inline box3f getWedgeBounds(const size_t ID) const
    {
      return box3f()
        .including(vertex[wedges[ID].front.x])
        .including(vertex[wedges[ID].front.y])
        .including(vertex[wedges[ID].front.z])
        .including(vertex[wedges[ID].back.x])
        .including(vertex[wedges[ID].back.y])
        .including(vertex[wedges[ID].back.z]);
    }
    
    inline box3f getHexBounds(const size_t ID) const
    {
      const box3f b = box3f()
        .including(vertex[hexes[ID].top.x])
        .including(vertex[hexes[ID].top.y])
        .including(vertex[hexes[ID].top.z])
        .including(vertex[hexes[ID].top.w])
        .including(vertex[hexes[ID].base.x])
        .including(vertex[hexes[ID].base.y])
        .including(vertex[hexes[ID].base.z])
        .including(vertex[hexes[ID].base.w]);
      return b;
    }
    
    inline box3f getBounds(const PrimRef &pr) const
    {
      switch(pr.type) {
      case TET:  return getTetBounds(pr.ID);
      case PYR:  return getPyrBounds(pr.ID);
      case WEDGE: return getWedgeBounds(pr.ID);
      case HEX:  return getHexBounds(pr.ID);
      default: 
        throw std::runtime_error("not implemented");
      };
    }

    inline box4f getBounds4f(const PrimRef &pr) const
    {
      box3f   spatial = getBounds(pr);
      range1f value   = getValueRange(pr);
      return box4f(vec4f(spatial.lower,value.lower),
                   vec4f(spatial.upper,value.upper));
    }
    
    void finalize()
    {
      if (perVertex) perVertex->finalize();
      if (perHex)    perHex->finalize();
      if (perTet)    perTet->finalize();
      bounds = box3f();
      std::mutex mutex;
      parallel_for_blocked(0,vertex.size(),16*1024,[&](size_t begin, size_t end) {
                                                     box3f rangeBounds;
                                                     for (size_t i=begin;i<end;i++)
                                                       rangeBounds.extend(vertex[i]);
                                                     std::lock_guard<std::mutex> lock(mutex);
                                                     bounds.extend(rangeBounds);
                                                   });
    }

    /*! if data set has per-vertex data this won't change anything; if
      it has per-cell scalar values it will compute create a new
      per-vertex data field based on the averages of all adjacent
      cell scalars */
    void createPerVertexData();
    
    std::vector<vec3f> vertex;
    Attribute::SP      perVertex;
    Attribute::SP      perTet;
    Attribute::SP      perHex;
    
    // -------------------------------------------------------
    // surface elements:
    // -------------------------------------------------------
    std::vector<vec3i> triangles;
    std::vector<vec4i> quads;

    // -------------------------------------------------------
    // volume elements:
    // -------------------------------------------------------
    std::vector<Tet>   tets;
    std::vector<Pyr>   pyrs;
    std::vector<Wedge> wedges;
    std::vector<Hex>   hexes;

    /*! in some cases, it makes sense to allow for storing a
      user-provided per-vertex tag (may be empty) */
    std::vector<size_t> vertexTag;
    
    box3f   bounds;
  };

  /*! helper clas that allows to create a new umesh's vertex array
    from the vertices of one or more other meshes, allowing to find
    the duplicates and translating the input meshes' vertices to the
    new mesh's vertex indices */
  struct RemeshHelper {
    RemeshHelper(UMesh &target)
      : target(target)
    {}

    /*! given a vertex v, return its ID in the target mesh's vertex
      array (if present), or add it (if not). To afterwards allow the
      using libnray to look up which of the inptu vertices ended up
      where we also allow to specify a vertex "tag" for each input
      vertex. meshes should only ever get built with *either* this
      functoin of the one that uses a float scalar, not mixed */
    uint32_t getID(const vec3f &v, size_t tag=0)
    {
      auto it = knownVertex.find(v);
      if (it != knownVertex.end()) {
        return it->second;
      }
      int ID = target.vertex.size();
      knownVertex[v] = ID;
      target.vertexTag.push_back(tag);
      target.vertex.push_back(v);
      return ID;
    }

    /*! given a vertex v and associated per-vertex scalar value s,
      return its ID in the target mesh's vertex array (if present), or
      add it (if not). To afterwards allow the using libnray to look
      up which of the inptu vertices ended up where we also allow to
      specify a vertex "tag" for each input vertex.  meshes should
      only ever get built with *either* this functoin of the one that
      uses a size_t tag, not mixed */
    uint32_t getID(const vec3f &v, float scalar)
    {
      auto it = knownVertex.find(v);
      if (it != knownVertex.end()) {
        return it->second;
      }
      int ID = target.vertex.size();
      knownVertex[v] = ID;
      if (!target.perVertex)
        target.perVertex = std::make_shared<Attribute>();
      target.perVertex->value.push_back(scalar);
      target.vertex.push_back(v);
      return ID;
    }

    /*! given a vertex ID in another mesh, return an ID for the
        *output* mesh that corresponds to this vertex (add to output
        if not already present) */
    uint32_t translate(const uint32_t in,
                       UMesh::SP otherMesh)
    {
      if (otherMesh->perVertex) {
        return getID(otherMesh->vertex[in],
                     otherMesh->perVertex->value[in]);
      } else if (!otherMesh->vertexTag.empty()) {
        return getID(otherMesh->vertex[in],
                     otherMesh->vertexTag[in]);
      } else 
        throw std::runtime_error("can't translate a vertex from another mesh that has neither scalars not vertex tags");
    }
    
    void translate(uint32_t *indices, int N,
                   UMesh::SP otherMesh)
    {
      for (int i=0;i<N;i++)
        indices[i] = translate(indices[i],otherMesh);
    }

    void add(UMesh::SP otherMesh, UMesh::PrimRef primRef)
    {
      switch (primRef.type) {
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
    
    std::map<vec3f,uint32_t> knownVertex;
    
    UMesh &target;
    // std::vector<size_t> vertexTag;
  };
  
  /*! helper functoin for printf debugging - puts the four elemnt
      sizes into a human-readable (short) string*/
  std::string sizeString(UMesh::SP mesh);
  
} // ::tetty
