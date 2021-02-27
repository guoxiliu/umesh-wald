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

#include "umesh/extractShellFaces.h"
#include "umesh/RemeshHelper.h"
# ifdef UMESH_HAVE_TBB
#  include "tbb/parallel_sort.h"
# endif
#include <set>
#include <algorithm>
#include <string.h>

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

namespace umesh {

  // UMesh::SP dbg_input;

  /*! given a umesh with mixed volumetric elements, create a a new
      mesh of surface elemnts (ie, triangles and quads) that
      corresponds to the outside facing "shell" faces of the input
      elements (ie, all those that re not shared by two different
      elements. All surface elements in the output mesh will be
      INWARD facing. */
  UMesh::SP extractShellFaces(UMesh::SP mesh);
} // ::umesh

namespace umesh {
  using std::swap;
  
  struct PrimFacetRef{
    /*! only 4 possible types (tet, pyr, wedge, or hex) */
    uint64_t primType:3;
    /*! only 8 possible facet it could be (in a hex) */
    uint64_t facetIdx:3;
    int64_t  primIdx:58;
  };
  
  std::ostream &operator<<(std::ostream &out, const PrimFacetRef &ref)
  {
    out << "Ref{type="<<ref.primType<<",fct="<<ref.facetIdx<<",primID="<<ref.primIdx<<"}";
    return out;
  }
  

  struct Facet {
    vec4i        vertexIdx;
    PrimFacetRef prim;
    int          orientation;
  };

  std::ostream &operator<<(std::ostream &out, const Facet &facet)
  {
    out << "Facet{vtx="<<facet.vertexIdx<<",prim="<<facet.prim<<",orientation="<<facet.orientation<<"}";
    return out;
  }
  
  struct FacetComparator {
    inline 
    bool operator()(const Facet &a, const Facet &b) const {
      return
        (a.vertexIdx.x < b.vertexIdx.x)
        ||
        ((a.vertexIdx.x == b.vertexIdx.x) &&
         (a.vertexIdx.y <  b.vertexIdx.y))
        ||
        ((a.vertexIdx.x == b.vertexIdx.x) &&
         (a.vertexIdx.y == b.vertexIdx.y) &&
         (a.vertexIdx.z <  b.vertexIdx.z))
        ||
        ((a.vertexIdx.x == b.vertexIdx.x) &&
         (a.vertexIdx.y == b.vertexIdx.y) &&
         (a.vertexIdx.z == b.vertexIdx.z) &&
         (a.vertexIdx.w <  b.vertexIdx.w));
    }
  };


  struct SharedFace {
    vec4i vertexIdx;
    PrimFacetRef onFront, onBack;
  };
               
  /*! describes input data through plain pointers, so we can run the
    same algorithm once with std::vector::data() (on the host) or
    cuda-malloced data (on gpu) */
  struct InputMesh {
    Tet   *tets;
    size_t numTets;
    Pyr   *pyrs;
    size_t numPyrs;
    Wedge *wedges;
    size_t numWedges;
    Hex   *hexes;
    size_t numHexes;
  };

  // ==================================================================
  // compute vertex order stage
  // ==================================================================
  
  inline 
  void computeUniqueVertexOrder(Facet &facet)
  {
    vec4i idx = facet.vertexIdx;
    std::set<int> uniqueIDs;
    uniqueIDs.insert(idx.x);
    uniqueIDs.insert(idx.y);
    uniqueIDs.insert(idx.z);
    
    if (idx.w < 0) {
      if (uniqueIDs.size() < 3) {
        facet.vertexIdx = vec4i(-1);
        return;
      }
        // throw std::runtime_error("tri face with "+std::to_string(uniqueIDs.size()) + "vertices!?");
      if (idx.y < idx.x)
        { swap(idx.x,idx.y); facet.orientation = 1-facet.orientation; }
      if (idx.z < idx.x)
        { swap(idx.x,idx.z); facet.orientation = 1-facet.orientation; }
      if (idx.z < idx.y)
        { swap(idx.y,idx.z); facet.orientation = 1-facet.orientation; }
    } else {
      uniqueIDs.insert(idx.w);

      if (uniqueIDs.size() == 2) {
        facet.vertexIdx = vec4i(-1);
        return;
      }
      
      if (uniqueIDs.size() == 3) {
        if (idx.x==idx.y) {
          idx = { idx.x, idx.z, idx.w, -1 };
        } else if (idx.x == idx.z) {
          // oooooh... this one is fishy
          PING;
          idx = { idx.x, idx.y, idx.w, -1 };
        } else if (idx.x == idx.w) {
          idx = { idx.x, idx.y, idx.z, -1 };
        } else if (idx.y == idx.z) {
          idx = { idx.x, idx.y, idx.w, -1 };
        } else if (idx.y == idx.w) {
          // oooooh... this one is fishy
          PING;
          idx = { idx.x, idx.y, idx.z, -1 };
        } else if (idx.z == idx.w) {
          idx = { idx.x, idx.y, idx.z, -1 };
        } else throw std::runtime_error("???");
        
        if (idx.y < idx.x)
          { swap(idx.x,idx.y); facet.orientation = 1-facet.orientation; }
        if (idx.z < idx.x)
          { swap(idx.x,idx.z); facet.orientation = 1-facet.orientation; }
        if (idx.z < idx.y)
          { swap(idx.y,idx.z); facet.orientation = 1-facet.orientation; }
      } else {

        int lv = idx.x, li=0;
        if (idx.y < lv) { lv = idx.y; li = 1; }
        if (idx.z < lv) { lv = idx.z; li = 2; }
        if (idx.w < lv) { lv = idx.w; li = 3; }

        switch (li) {
        case 0: idx = { idx.x,idx.y,idx.z,idx.w }; break;
        case 1: idx = { idx.y,idx.z,idx.w,idx.x }; break;
        case 2: idx = { idx.z,idx.w,idx.x,idx.y }; break;
        case 3: idx = { idx.w,idx.x,idx.y,idx.z }; break;
        };

        if (idx.w < idx.y) {
          facet.orientation = 1-facet.orientation;
          swap(idx.w,idx.y);
        }
      }
    }
    facet.vertexIdx = idx;
  }

  void computeUniqueVertexOrder(Facet *facets, size_t numFacets)
  {
    parallel_for_blocked
      (0,numFacets,1024,
       [&](size_t begin, size_t end) {
         for (size_t i=begin;i<end;i++) {
           computeUniqueVertexOrder(facets[i]);
         }
       });
  }

  // ==================================================================
  // init faces
  // ==================================================================
  
  inline 
  void writeTetFacets(Facet *facets,
                      size_t tetIdx,
                      InputMesh mesh
                      )
  {
    for (int i=0;i<4;i++) facets[i].prim.primType = UMesh::TET;
    for (int i=0;i<4;i++) facets[i].prim.facetIdx = i;
    for (int i=0;i<4;i++) facets[i].prim.primIdx  = tetIdx;
    for (int i=0;i<4;i++) facets[i].orientation   = 0;

    vec4i tet = mesh.tets[tetIdx];

    facets[0].vertexIdx = { tet.y,tet.w,tet.z,-1 };
    facets[1].vertexIdx = { tet.x,tet.z,tet.w,-1 };
    facets[2].vertexIdx = { tet.x,tet.w,tet.y,-1 };
    facets[3].vertexIdx = { tet.x,tet.y,tet.z,-1 };
  }
  
  inline 
  void writePyrFacets(Facet *facets,
                      size_t pyrIdx,
                      InputMesh mesh
                      )
  {
    for (int i=0;i<5;i++) facets[i].prim.primType = UMesh::PYR;
    for (int i=0;i<5;i++) facets[i].prim.facetIdx = i;
    for (int i=0;i<5;i++) facets[i].prim.primIdx  = pyrIdx;
    for (int i=0;i<5;i++) facets[i].orientation   = 0;
    
    UMesh::Pyr pyr = mesh.pyrs[pyrIdx];
    vec4i base = pyr.base;
// #if 0
//     int i0 = pyr.base.x;
//     int i1 = pyr.base.y;
//     int i2 = pyr.base.z;
//     int i3 = pyr.base.w;
//     int i4 = pyr.top;
//     facets[0].vertexIdx = { i0,i1,i4,-1 };
//     facets[0].vertexIdx = { i0,i4,i3,-1 };
//     facets[0].vertexIdx = { i1,i2,i4,-1 };
//     facets[0].vertexIdx = { i3,i4,i2,-1 };
//     facets[4].vertexIdx = { i0,i3,i2,i1 };
// #else
    facets[0].vertexIdx = { pyr.top,base.y,base.x,-1 };
    facets[1].vertexIdx = { pyr.top,base.z,base.y,-1 };
    facets[2].vertexIdx = { pyr.top,base.w,base.z,-1 };
    facets[3].vertexIdx = { pyr.top,base.x,base.w,-1 };
    facets[4].vertexIdx = { base.x,base.y,base.z,base.w };
// #endif
  }
  
  inline 
  void writeWedgeFacets(Facet *facets,
                        size_t wedgeIdx,
                        InputMesh mesh
                        )
  {
    for (int i=0;i<5;i++) facets[i].prim.primType = UMesh::WEDGE;
    for (int i=0;i<5;i++) facets[i].prim.facetIdx = i;
    for (int i=0;i<5;i++) facets[i].prim.primIdx  = wedgeIdx;
    for (int i=0;i<5;i++) facets[i].orientation   = 0;
    
    UMesh::Wedge wedge = mesh.wedges[wedgeIdx];
    int i0 = wedge.front.x;
    int i1 = wedge.front.y;
    int i2 = wedge.front.z;
    int i3 = wedge.back.x;
    int i4 = wedge.back.y;
    int i5 = wedge.back.z;

    // std::set<int> uniqueIndices;
    // uniqueIndices.insert(i0);
    // uniqueIndices.insert(i1);
    // uniqueIndices.insert(i2);
    // uniqueIndices.insert(i3);
    // uniqueIndices.insert(i4);
    // uniqueIndices.insert(i5);

    // if (uniqueIndices.size() == 6) {
      facets[0].vertexIdx = { i0,i2,i1,-1 };
      facets[1].vertexIdx = { i3,i4,i5,-1 };
      facets[2].vertexIdx = { i0,i3,i5,i2 };
      facets[3].vertexIdx = { i1,i2,i5,i4 };
      facets[4].vertexIdx = { i0,i1,i4,i3 };
    // } else if (uniqueIndices.size() == 5) {
    //   if (i0 == i1) {
    //     // front face is a straight edge...
        
    //     // left (curved)
    //     facets[0].vertexIdx = { i0,i3,i5,i2 };
    //     // right (curved)
    //     facets[1].vertexIdx = { i1,i2,i5,i4 };
    //     // front - empty
    //     facets[2].vertexIdx = { -1,-1,-1,-1 };
    //     // back - tri
    //     facets[3].vertexIdx = { i3,i4,i5,-1 };
    //     // bottom - tri
    //     facets[4].vertexIdx = { i0,i4,i3,-1 };
    //   } else if (i3 == i4) {
    //     // back face is a straight edge...
        
    //     // left (curved)
    //     facets[0].vertexIdx = { i0,i3,i5,i2 };
    //     // right (curved)
    //     facets[1].vertexIdx = { i1,i2,i5,i4 };
    //     // front - tri
    //     facets[2].vertexIdx = { i0,i2,i1,-1 };
    //     // back - empty
    //     facets[3].vertexIdx = { -1,-1,-1,-1 };
    //     // bottom - tri
    //     facets[4].vertexIdx = { i0,i1,i3,-1 };
    //   } else
    //     throw std::runtime_error("case not handled (wedge 5)...");
    // } else
    //   throw std::runtime_error("case not handled (wedge < 5)...");
  }
  
  inline 
  void writeHexFacets(Facet *facets,
                      size_t hexIdx,
                      InputMesh mesh
                      )
  {
    for (int i=0;i<6;i++) facets[i].prim.primType = UMesh::HEX;
    for (int i=0;i<6;i++) facets[i].prim.facetIdx = i;
    for (int i=0;i<6;i++) facets[i].prim.primIdx  = hexIdx;
    for (int i=0;i<6;i++) facets[i].orientation   = 0;
    
    UMesh::Hex hex = mesh.hexes[hexIdx];
    int i0 = hex.base.x;
    int i1 = hex.base.y;
    int i2 = hex.base.z;
    int i3 = hex.base.w;
    int i4 = hex.top.x;
    int i5 = hex.top.y;
    int i6 = hex.top.z;
    int i7 = hex.top.w;

    facets[0].vertexIdx = { i0,i1,i2,i3 };
    facets[1].vertexIdx = { i4,i7,i6,i5 };
    facets[2].vertexIdx = { i0,i4,i5,i1 };
    facets[3].vertexIdx = { i2,i6,i7,i3 };
    facets[4].vertexIdx = { i1,i5,i6,i2 };
    facets[5].vertexIdx = { i0,i3,i7,i4 };
  }
  
  inline 
  void writeFacets(Facet *facets, size_t jobIdx, const InputMesh &mesh)
  {
    // write tets
    if (jobIdx < mesh.numTets) {
      writeTetFacets(facets+4*jobIdx,jobIdx,mesh);
      return;
    }
    facets += 4*mesh.numTets;
    jobIdx -= mesh.numTets;
  
    // write pyramids
    if (jobIdx < mesh.numPyrs) {
      writePyrFacets(facets+5*jobIdx,jobIdx,mesh);
      return;
    }
    facets += 5*mesh.numPyrs;
    jobIdx -= mesh.numPyrs;
  
    // write wedges
    if (jobIdx < mesh.numWedges) {
      writeWedgeFacets(facets+5*jobIdx,jobIdx,mesh);
      return;
    }
    facets += 5*mesh.numWedges;
    jobIdx -= mesh.numWedges;
  
    // write hexes
    if (jobIdx < mesh.numHexes) {
      writeHexFacets(facets+6*jobIdx,jobIdx,mesh);
      return;
    }
    return;
  }

  void writeFacets(Facet *facets,
                   const InputMesh &mesh)
  {
    size_t numPrims
      = mesh.numTets
      + mesh.numPyrs
      + mesh.numWedges
      + mesh.numHexes;
    parallel_for_blocked
      (0,numPrims,1024,
       [&](size_t begin, size_t end) {
         for (size_t i=begin;i<end;i++)
           writeFacets(facets,i,mesh);
       });
  }

  // ==================================================================
  // sort facet array
  // ==================================================================
  void sortFacets(Facet *facets, size_t numFacets)
  {
# ifdef UMESH_HAVE_TBB
    tbb::parallel_sort(facets,facets+numFacets,FacetComparator());
# else
    std::sort(facets,facets+numFacets,FacetComparator());
# endif
  }
  
  // ==================================================================
  // set up / upload input mesh data
  // ==================================================================
  template<typename T>
  inline void upload(T *&ptr, size_t &count,
                     const std::vector<T> &vec)
  {
    ptr = (T*)vec.data();
    count = vec.size();
  }
  inline void freeInput(InputMesh &mesh)
  {}
  Facet *allocateFacets(size_t numFacets) { return new Facet[numFacets]; }
  void freeFacets(Facet *facets)
  { delete[] facets; }

  void setupInput(InputMesh &mesh, UMesh::SP input)
  {
    upload(mesh.tets,mesh.numTets,input->tets);
    upload(mesh.pyrs,mesh.numPyrs,input->pyrs);
    upload(mesh.wedges,mesh.numWedges,input->wedges);
    upload(mesh.hexes,mesh.numHexes,input->hexes);
  }
  

  // ==================================================================
  // let facets write the facess
  // ==================================================================
  inline 
  void facetsWriteFacesKernel(SharedFace *faces,
                             const Facet *facets,
                             const uint64_t *faceIndices,
                             size_t facetIdx)
  {
    const Facet facet = facets[facetIdx];

    if (facet.vertexIdx.x < 0)
      return;
    
    assert(faceIndices[facetIdx] > 0);
    size_t faceIdx = faceIndices[facetIdx]-1;
    SharedFace &face = faces[faceIdx];
    auto &side = facet.orientation ? face.onFront : face.onBack;
    face.vertexIdx = facet.vertexIdx;
    if (!(side.primIdx < 0)) {
      PRINT(facet);
      PRINT(facetIdx);
      PRINT(faceIdx);
      PRINT(face.onFront);
      PRINT(face.onBack);
      PRINT(side);
      throw std::runtime_error("side is used twice!?");
    }
    side = facet.prim;
  }
  
  void facetsWriteFaces(SharedFace *faces,
                        const Facet *facets,
                        const uint64_t *faceIndices,
                        size_t numFacets)
  {
#if 0
    for (size_t i=0;i<numFacets;i++)
      facetsWriteFacesKernel(faces,facets,faceIndices,i);
#else
    parallel_for_blocked
      (0,numFacets,1024,
       [&](size_t begin, size_t end) {
         for (size_t i=begin;i<end;i++)
           facetsWriteFacesKernel(faces,facets,faceIndices,i);
       });
#endif
  }
  
  // ==================================================================
  // manage mem for faces
  // ==================================================================
  SharedFace *allocateFaces(std::vector<SharedFace> &result,
                            size_t numFaces)
  {
    result.resize(numFaces);
    return result.data();
  }
  
  void finishFaces(std::vector<SharedFace> &result,
                   SharedFace *faces,
                   size_t numFaces)
  {
    /* nothing to do */
  }

  // ==================================================================
  // manage mem for face indices
  // ==================================================================

  void freeIndices(uint64_t *faceIndices)
  {
    delete[] faceIndices;
  }
  uint64_t *allocateIndices(size_t numFacets)
  {
    return new uint64_t[numFacets];
  }
  
  // ==================================================================
  // compute face indices from (sorted) facet array
  // ==================================================================

  inline 
  void initFaceIndexKernel(uint64_t *faceIndices,
                           Facet *facets,
                           size_t facetIdx)
  {
    faceIndices[facetIdx]
      =  (facetIdx == 0)
      || (facets[facetIdx-1].vertexIdx.x != facets[facetIdx].vertexIdx.x)
      || (facets[facetIdx-1].vertexIdx.y != facets[facetIdx].vertexIdx.y)
      || (facets[facetIdx-1].vertexIdx.z != facets[facetIdx].vertexIdx.z)
      || (facets[facetIdx-1].vertexIdx.w != facets[facetIdx].vertexIdx.w);
  }

  void clearFaces(SharedFace *faces, size_t numFaces)
  {
    PrimFacetRef clearPrim = { 0,0,-1 };
    clearPrim.primIdx = -1;
    for (size_t i=0;i<numFaces;i++) {
      faces[i].onFront = faces[i].onBack = clearPrim;
      faces[i].vertexIdx = vec4i(-1);
    }
  }
  void initFaceIndices(uint64_t *faceIndices,
                       Facet *facets,
                       size_t numFacets)
  {
    for (size_t i=0;i<numFacets;i++) {
      initFaceIndexKernel(faceIndices,facets,i);
    }
  }
  /*! not parallelized... this will likely be mem bound, anyway */
  void prefixSum(uint64_t *faceIndices,
                 size_t numFacets)
  {
    size_t sum = 0;
    for (size_t i=0;i<numFacets;i++) {
      size_t old = faceIndices[i];
      faceIndices[i] = sum;
      sum += old;
    }
  }
  void postfixSum(uint64_t *faceIndices,
                 size_t numFacets)
  {
    size_t sum = 0;
    for (size_t i=0;i<numFacets;i++) {
      size_t old = faceIndices[i];
      sum += old;
      faceIndices[i] = sum;
    }
  }


  // ==================================================================
  // aaaand ... wrap it all together
  // ==================================================================

  std::vector<SharedFace> computeFaces(UMesh::SP input)
  {
    assert(input);
    std::chrono::steady_clock::time_point
      begin_inc = std::chrono::steady_clock::now();
    InputMesh mesh;
    setupInput(mesh,input);

    std::chrono::steady_clock::time_point
      begin_exc = std::chrono::steady_clock::now();
    
    // -------------------------------------------------------
    size_t numFacets
      = 4 * mesh.numTets
      + 5 * mesh.numPyrs
      + 5 * mesh.numWedges
      + 6 * mesh.numHexes;
    if (numFacets == 0)
      /*! this is critical: without this we'll be getting a crash
          later on when it tries to access the "last" facet */
      return {};
    
    Facet *facets = allocateFacets(numFacets);
    writeFacets(facets,mesh);
    // for (int i=0;i<numFacets;i++)
    //   std::cout << "facet " << i << " = " << facets[i] << std::endl;
    computeUniqueVertexOrder(facets,numFacets);

    // -------------------------------------------------------
    sortFacets(facets,numFacets);
    uint64_t *faceIndices = allocateIndices(numFacets);
    initFaceIndices(faceIndices,facets,numFacets);
    // prefixSum(faceIndices,numFacets);
    postfixSum(faceIndices,numFacets);
    // -------------------------------------------------------
    size_t numFaces = faceIndices[numFacets-1];//+1;
    std::vector<SharedFace> result;
    SharedFace *faces = allocateFaces(result,numFaces);
    clearFaces(faces,numFaces);
    
    // -------------------------------------------------------
    facetsWriteFaces(faces,facets,faceIndices,numFacets);
    
    finishFaces(result,faces,numFaces);
    freeIndices(faceIndices);
    freeFacets(facets);
    return result;
  }

#if 0
  // only use for debugging, to force priting of prims that contain certain vertices or faces
  template<typename T>
  inline bool contains(T t, int ID)
  {
    for (int i=0;i<t.numVertices;i++)
      if (t[i] == ID) return true;
    return false;
  }
  
  template<typename T>
  inline bool offending(T t)
  {
    return
      contains(t,0) &&
      contains(t,21) &&
      contains(t,22) &&
      contains(t,23);
  }
#endif
  
  /*! given a umesh with mixed volumetric elements, create a a new
      mesh of surface elemnts (ie, triangles and quads) that
      corresponds to the outside facing "shell" faces of the input
      elements (ie, all those that re not shared by two different
      elements. All surface elements in the output mesh will be
      OUTWARD facing. */
  UMesh::SP extractShellFaces(UMesh::SP input,
                              /*! if true, we'll create a new set of
                                vertices for ONLY the required
                                vertices. If false, we'll leave the
                                vertices array empty, and have the
                                vertex indices refer to the
                                original input mesh */
                              bool remeshVertices)
  {
#if 0
    // dbg_input = input;
    assert(input);
    for (auto prim : input->tets)
      if (offending(prim)) {
        PRINT(prim);
        for (int i=0;i<prim.numVertices;i++)
          PRINT(input->vertices[prim[i]]);
      }
    for (auto prim : input->pyrs)
      if (offending(prim)) {
        PRINT(prim);
        for (int i=0;i<prim.numVertices;i++)
          PRINT(input->vertices[prim[i]]);
      }
    for (auto prim : input->wedges)
      if (offending(prim)) {
        PRINT(prim);
        for (int i=0;i<prim.numVertices;i++)
          PRINT(input->vertices[prim[i]]);
      }
    for (auto prim : input->hexes)
      if (offending(prim)) {
        PRINT(prim);
        for (int i=0;i<prim.numVertices;i++)
          PRINT(input->vertices[prim[i]]);
      }
#endif
    
    std::vector<SharedFace> faces
      = computeFaces(input);

    assert(faces.empty() || !input->vertices.empty());
    UMesh::SP output = std::make_shared<UMesh>();
    RemeshHelper helper(*output);
    for (auto &face: faces) {
      if (face.vertexIdx.x < 0)
        // invalid face
        continue;
      
      if (face.onFront.primIdx < 0 && face.onBack.primIdx < 0) {
        PRINT(face.vertexIdx);
        throw std::runtime_error("face that has BOTH sides unused!?");
      } else if (face.onFront.primIdx < 0) {
        if (face.vertexIdx.w < 0) {
          // SWAP
          vec3i tri(face.vertexIdx.x,
                    face.vertexIdx.z,
                    face.vertexIdx.y);
          if (remeshVertices)
            helper.translate(&tri.x,3,input);
          output->triangles.push_back(tri);
        } else {
          // SWAP
          vec4i quad(face.vertexIdx.x,
                     face.vertexIdx.w,
                     face.vertexIdx.z,
                     face.vertexIdx.y);
          if (remeshVertices)
            helper.translate(&quad.x,4,input);
          output->quads.push_back(quad);
        }
      } else if (face.onBack.primIdx < 0) {
        if (face.vertexIdx.w < 0) {
          // NO SWAP
          vec3i tri(face.vertexIdx.x,
                    face.vertexIdx.y,
                    face.vertexIdx.z);
          if (remeshVertices)
            helper.translate(&tri.x,3,input);
          output->triangles.push_back(tri);
        } else {
          // NO SWAP
          vec4i quad(face.vertexIdx.x,
                     face.vertexIdx.y,
                     face.vertexIdx.z,
                     face.vertexIdx.w);
          if (remeshVertices)
            helper.translate(&quad.x,4,input);
          output->quads.push_back(quad);
        }
      } else {
        /* inner face ... ignore */
      }
    }
    // dbg_input = 0;
    return output;
  }
  
}
