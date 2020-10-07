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

    Attribute(int num=0) : values(num) {}
    
    /*! tells this attribute that its values are set, and precomputations can be done */
    void finalize();
    
    std::string name;
    /*! for now lets implement only float attributes. node/ele files
      actually seem to have ints in the ele file (if they have
      anything at all), but these should still fir into floats, so
      we should be good for now. This may change once we better
      understand where these values come from, and what they
      mean. */
    std::vector<float> values;
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

    /*! return a string of the form "UMesh{#tris=...}" */
    std::string toString() const;
    
    /*! print some basic info of this mesh to std::cout */
    void print();

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
        .including(perVertex->values[tets[ID].x])
        .including(perVertex->values[tets[ID].y])
        .including(perVertex->values[tets[ID].z])
        .including(perVertex->values[tets[ID].w]);
      return b;
    }
    
    inline range1f getPyrValueRange(const size_t ID) const
    {
      return range1f()
        .including(perVertex->values[pyrs[ID].top])
        .including(perVertex->values[pyrs[ID].base.x])
        .including(perVertex->values[pyrs[ID].base.y])
        .including(perVertex->values[pyrs[ID].base.z])
        .including(perVertex->values[pyrs[ID].base.w]);
    }
    
    inline range1f getWedgeValueRange(const size_t ID) const
    {
      return range1f()
        .including(perVertex->values[wedges[ID].front.x])
        .including(perVertex->values[wedges[ID].front.y])
        .including(perVertex->values[wedges[ID].front.z])
        .including(perVertex->values[wedges[ID].back.x])
        .including(perVertex->values[wedges[ID].back.y])
        .including(perVertex->values[wedges[ID].back.z])
        ;
    }
    
    inline range1f getHexValueRange(const size_t ID) const
    {
      const range1f b = range1f()
        .including(perVertex->values[hexes[ID].top.x])
        .including(perVertex->values[hexes[ID].top.y])
        .including(perVertex->values[hexes[ID].top.z])
        .including(perVertex->values[hexes[ID].top.w])
        .including(perVertex->values[hexes[ID].base.x])
        .including(perVertex->values[hexes[ID].base.y])
        .including(perVertex->values[hexes[ID].base.z])
        .including(perVertex->values[hexes[ID].base.w]);
      return b;
    }
    
    inline range1f getValueRange(const PrimRef &pr) const
    {
      switch(pr.type) {
      case TET:  return getTetValueRange(pr.ID);
      case PYR:  return getPyrValueRange(pr.ID);
      case WEDGE:return getWedgeValueRange(pr.ID);
      case HEX:  return getHexValueRange(pr.ID);
      default: 
        throw std::runtime_error("not implemented");
      };
    }



    
    

    inline box3f getTetBounds(const size_t ID) const
    {
      const box3f b = box3f()
        .including(vertices[tets[ID].x])
        .including(vertices[tets[ID].y])
        .including(vertices[tets[ID].z])
        .including(vertices[tets[ID].w]);
      return b;
    }
    
    inline box3f getPyrBounds(const size_t ID) const
    {
      return box3f()
        .including(vertices[pyrs[ID].top])
        .including(vertices[pyrs[ID].base.x])
        .including(vertices[pyrs[ID].base.y])
        .including(vertices[pyrs[ID].base.z])
        .including(vertices[pyrs[ID].base.w]);
    }
    
    inline box3f getWedgeBounds(const size_t ID) const
    {
      return box3f()
        .including(vertices[wedges[ID].front.x])
        .including(vertices[wedges[ID].front.y])
        .including(vertices[wedges[ID].front.z])
        .including(vertices[wedges[ID].back.x])
        .including(vertices[wedges[ID].back.y])
        .including(vertices[wedges[ID].back.z]);
    }
    
    inline box3f getHexBounds(const size_t ID) const
    {
      const box3f b = box3f()
        .including(vertices[hexes[ID].top.x])
        .including(vertices[hexes[ID].top.y])
        .including(vertices[hexes[ID].top.z])
        .including(vertices[hexes[ID].top.w])
        .including(vertices[hexes[ID].base.x])
        .including(vertices[hexes[ID].base.y])
        .including(vertices[hexes[ID].base.z])
        .including(vertices[hexes[ID].base.w]);
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

    /*! finalize a mesh, and compute min/max ranges where required */
    void finalize();

    /*! if data set has per-vertex data this won't change anything; if
      it has per-cell scalar values it will compute create a new
      per-vertex data field based on the averages of all adjacent
      cell scalars */
    void createPerVertexData();
    
    std::vector<vec3f> vertices;
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
  
  /*! helper functoin for printf debugging - puts the four elemnt
      sizes into a human-readable (short) string*/
  std::string sizeString(UMesh::SP mesh);
  
} // ::tetty
