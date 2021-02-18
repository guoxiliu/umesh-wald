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

#include "umesh/tetrahedralize.h"
#include <algorithm>

namespace umesh {
  struct MergedMesh {
    
    MergedMesh(UMesh::SP in)
      : in(in),
        out(std::make_shared<UMesh>())
    {
      out->vertices = in->vertices;
      if (in->perVertex) {
        out->perVertex = std::make_shared<Attribute>();
        out->perVertex->name   = in->perVertex->name;
        out->perVertex->values = in->perVertex->values;
      }
    }

    void add(const UMesh::Tet &tet, const std::string &dbg="")
    {
      if (tet.x == tet.y || tet.x == tet.z || tet.x == tet.w
          || tet.y == tet.z || tet.y == tet.w
          || tet.z == tet.w)
        return;
      vec3f a = out->vertices[tet.x];
      vec3f b = out->vertices[tet.y];
      vec3f c = out->vertices[tet.z];
      vec3f d = out->vertices[tet.w];
      float volume = dot(d-a,cross(b-a,c-a));
      if (volume == 0.f) return;

      if (volume < 0.f) {
        static bool warned = false;
        if (!warned) {
          std::cout
            << UMESH_TERMINAL_RED
            <<"WARNING: at least one tet (or other element that generated a tet)\n was wrongly oriented!!! (I'll swap those tets, but that's still fishy...)"
            << UMESH_TERMINAL_DEFAULT
            << std::endl;
          warned = true;
        }
        out->tets.push_back({tet.x,tet.y,tet.w,tet.z});
      }
      out->tets.push_back(tet);
    }

    void add(const UMesh::Pyr &pyr, const std::string &dbg="")
    {
      int base = getCenter({pyr[0],pyr[1],pyr[2],pyr[3]});
      add(UMesh::Tet(pyr[0],pyr[1],base,pyr[4]),dbg+"pyr0");
      add(UMesh::Tet(pyr[1],pyr[2],base,pyr[4]),dbg+"pyr1");
      add(UMesh::Tet(pyr[2],pyr[3],base,pyr[4]),dbg+"pyr2");
      add(UMesh::Tet(pyr[3],pyr[0],base,pyr[4]),dbg+"pyr3");
    }

    void add(const UMesh::Wedge &wedge)
    {
      // newly created points:
      int center = getCenter({wedge[0],wedge[1],wedge[2],
                              wedge[3],wedge[4],wedge[5]});

      // bottom face to center
      add(UMesh::Pyr(wedge[0],wedge[1],wedge[4],wedge[3],center),"wed1");
      // left face to center
      add(UMesh::Pyr(wedge[0],wedge[3],wedge[5],wedge[2],center),"wed2");
      // right face to center
      add(UMesh::Pyr(wedge[1],wedge[2],wedge[5],wedge[4],center),"wed3");
      // front face to center
      add(UMesh::Tet(wedge[0],wedge[2],wedge[1],center),"wed4");
      // back face to center
      add(UMesh::Tet(wedge[3],wedge[4],wedge[5],center),"wed5");
    }
    
    void add(const UMesh::Hex &hex)
    {
      // newly created points:
      int center = getCenter({hex[0],hex[1],hex[2],hex[3],
                              hex[4],hex[5],hex[6],hex[7]});

      // bottom face to center
      add(UMesh::Pyr(hex[0],hex[1],hex[2],hex[3],center));
      // top face to center
      add(UMesh::Pyr(hex[4],hex[7],hex[6],hex[5],center));

      // front face to center
      add(UMesh::Pyr(hex[0],hex[4],hex[5],hex[1],center));
      // back face to center
      add(UMesh::Pyr(hex[2],hex[6],hex[7],hex[3],center));

      // left face to center
      add(UMesh::Pyr(hex[0],hex[3],hex[7],hex[4],center));
      // right face to center
      add(UMesh::Pyr(hex[1],hex[5],hex[6],hex[2],center));
    }
    
    int getCenter(std::vector<int> idx)
    {
      std::sort(idx.begin(),idx.end());
      auto it = newVertices.find(idx);
      if (it != newVertices.end())
        return it->second;
      int ID = (int)out->vertices.size();
      
      vec3f centerPos = vec3f(0.f);
      float centerVal = 0.f;
      for (auto i : idx) {
        if (in->perVertex)
          centerVal += in->perVertex->values[i];
        centerPos = centerPos + in->vertices[i];
      }
      centerVal *= (1.f/idx.size());
      centerPos = centerPos * (1.f/idx.size());
      
      newVertices[idx] = ID;
      out->vertices.push_back(centerPos);
      if (out->perVertex)
        out->perVertex->values.push_back(centerVal);
      return ID;
    }

    std::map<vec3f,int> vertices;
    std::map<std::vector<int>,int> newVertices;
    UMesh::SP in, out;
  };

  /*! tetrahedralize a given input mesh; in a way that non-triangular
    faces (from wedges, pyramids, and hexes) will always be
    subdividied exactly the same way even if that same face is used
    by another elemnt with different vertex order
  
    Notes:

    - tetrahedralization will create new vertices; if the input
    contained a scalar field the output will contain the same
    scalars for existing vertices, and will interpolate for newly
    created ones.
      
    - output will *not* contain the input's surface elements
    
    - the input's vertices will be fully contained within the output's
    vertex array, with the same indices; so any surface elements
    defined in the input should remain valid in the output's vertex
    array.
  */
  UMesh::SP tetrahedralize(UMesh::SP in)
  {
    MergedMesh merged(in);
    for (auto tet : in->tets)
      merged.add(tet);
    for (auto pyr : in->pyrs)
      merged.add(pyr);
    for (auto wedge : in->wedges) 
      merged.add(wedge);
    for (auto hex : in->hexes) 
      merged.add(hex);
    std::cout << "done tetrahedralizing, got "
              << sizeString(merged.out)
              << " from " << sizeString(in) << std::endl;
    return merged.out;
  }


  /*! same as tetrahedralize(umesh), but will, eventually, contain
      only the tetrahedra created by the 'owned' elements listed, EVEN
      THOUGH the vertex array produced by that will be the same as in
      the original tetrahedralize(mesh) version */
  UMesh::SP tetrahedralize(UMesh::SP in,
                           int ownedTets,
                           int ownedPyrs,
                           int ownedWedges,
                           int ownedHexes)
  {
    // ------------------------------------------------------------------
    // first stage: push _all_ elements, to ensure we get same vertex
    // array as 'non-owned' version
    // ------------------------------------------------------------------
    MergedMesh merged(in);
    for (auto tet : in->tets)
      merged.add(tet);
    for (auto pyr : in->pyrs)
      merged.add(pyr);
    for (auto wedge : in->wedges) 
      merged.add(wedge);
    for (auto hex : in->hexes) 
      merged.add(hex);
    // ------------------------------------------------------------------
    // now, kll all so-far generated tets in the output mesh, but keep
    // the vertex array...
    // ------------------------------------------------------------------
    merged.out->tets.clear();
    // ------------------------------------------------------------------
    // and re-push (only) the owned ones
    // ------------------------------------------------------------------
    for (size_t i=0;i<std::min((int)in->tets.size(),ownedTets);i++)
      merged.add(in->tets[i]);
    for (size_t i=0;i<std::min((int)in->pyrs.size(),ownedPyrs);i++)
      merged.add(in->pyrs[i]);
    for (size_t i=0;i<std::min((int)in->wedges.size(),ownedWedges);i++)
      merged.add(in->wedges[i]);
    for (size_t i=0;i<std::min((int)in->hexes.size(),ownedHexes);i++)
      merged.add(in->hexes[i]);
    std::cout << "done tetrahedralizing (second stage), got "
              << sizeString(merged.out)
              << " from " << sizeString(in) << std::endl;
    return merged.out;
  }

  
} // ::umesh
