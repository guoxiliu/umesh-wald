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

#include "umesh/UMesh.h"
#include "umesh/io/IO.h"
// #include "tetty/UMesh.h"
#include <set>
#include <map>
#include <fstream>
#include <atomic>

#define DEBUG 0

#if DEBUG
# define DBG(a) a
#else
# define DBG(a) /**/
#endif

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
  std::atomic<uint64_t> numTets;
  std::atomic<uint64_t> numWedges;
  std::atomic<uint64_t> numWedgesPerfect;
  std::atomic<uint64_t> numWedgesTwisted;

  std::atomic<uint64_t> numPyramids;
  std::atomic<uint64_t> numPyramidsPerfect;
  std::atomic<uint64_t> numPyramidsTwisted;

  std::atomic<uint64_t> numHexes;
  std::atomic<uint64_t> numHexesPerfect;
  std::atomic<uint64_t> numHexesTwisted;

}
  inline bool operator<(const umesh::Tet &a, const umesh::Tet &b)
  {
    const uint64_t *pa = (const uint64_t *)&a;
    const uint64_t *pb = (const uint64_t *)&b;
    const bool res =  (pa[0] < pb[0]) || ((pa[0] == pb[0]) && (pa[1] < pb[1]));
    return res;
  }
  // namespace std {
  //   inline bool operator<(const umesh::vec4f a, const umesh::vec4f b)
  //   {
  //     for (int i=0;i<4;i++) {
  //       if (a[i] < b[i]) return true;
  //       if (a[i] > b[i]) return false;
  //     }
  //     return false;
  //   }
  // }

namespace umesh {
  struct Exa {
    struct LogicalCell {
      inline box3f bounds() const
      { return box3f(vec3f(pos),vec3f(pos+vec3i(1<<level))); }
      inline LogicalCell neighbor(const vec3i &delta) const
      { return { pos+delta*(1<<level),level }; }
      
      inline vec3f center() const { return vec3f(pos) + vec3f(0.5f*(1<<level)); }
      vec3i pos;
      int   level;
    };
  
    struct Cell : public LogicalCell {
      float scalar;
    };
  
    void add(Cell cell)
    {
      // cells[cell] = (int)cellList.size();
      cellList.push_back(cell);
      // if (cells.size() != cellList.size())
      //   throw std::runtime_error("bug in add(cell)");
      minLevel = min(minLevel,cell.level);
      maxLevel = max(maxLevel,cell.level);
      bounds.extend(cell.bounds());
    }

    size_t size() const { return cellList.size(); }

    box3f bounds;
    int minLevel=100, maxLevel=0;
    // stores cell and ID
    std::vector<Cell>  cellList;
    // std::map<Cell,size_t> cells;
    
    bool find(int &cellID, const vec3f &pos) const;
  };

  inline bool operator<(const Exa::LogicalCell &a, const Exa::LogicalCell &b)
  {
    const uint64_t *pa = (uint64_t *)&a;
    const uint64_t *pb = (uint64_t *)&b;
    return
      (pa[0] < pb[0])
      || (pa[0] == pb[0] && pa[1] < pb[1]);
  }

  inline bool operator==(const Exa::LogicalCell &a, const Exa::LogicalCell &b)
  {
    const uint64_t *pa = (uint64_t *)&a;
    const uint64_t *pb = (uint64_t *)&b;
    return pa[0] == pb[0] && pa[1] == pb[1];
  }

  inline bool operator<(const Exa::Cell &a, const Exa::Cell &b)
  {
    const Exa::LogicalCell &la = a;
    const Exa::LogicalCell &lb = b;
  
    return (la < lb) || ((la==lb) && (a.scalar < b.scalar));
  }

  inline bool operator==(const Exa::Cell &a, const Exa::Cell &b)
  {
    return ((const Exa::LogicalCell &)a == (const Exa::LogicalCell &)b) && (a.scalar == b.scalar);
  }


  inline bool operator!=(const Exa::Cell &a, const Exa::Cell &b)
  {
    return !(a == b);
  }

  using namespace std;
  
  std::ostream &operator<<(std::ostream &out, const Exa::LogicalCell &cell)
  {
    out << "[" << cell.pos << ","<<cell.level <<"]";
    return out;
  }

  std::ostream &operator<<(std::ostream &out, const Exa::Cell &cell)
  {
    out << "[" << cell.pos << ","<<cell.level <<";" << cell.scalar << "]";
    return out;
  }


  int lowerOnLevel(float f, int level)
  {
    f = floorf(f/(1<<level));
    return f*(1<<level);
  }

  // return vector-index of given cell, if exists, or -1
  bool Exa::find(int &result, const vec3f &where) const
  {
    // std::cout << "=======================================================" << std::endl;
    // // dbg = true;
    // PING;
    DBG(PING; PRINT(where));
    for (int level=minLevel;level<=maxLevel;level++) {
      // std::cout << "-------------------------------------------------------" << std::endl;
      int width = 1<<level;
      Cell query;
      query.pos.x = lowerOnLevel(where.x,level); //int(floorf(where.x)) & ~(width-1);
      query.pos.y = lowerOnLevel(where.y,level); //int(floorf(where.y)) & ~(width-1);
      query.pos.z = lowerOnLevel(where.z,level); //int(floorf(where.z)) & ~(width-1);
      query.level = level;
      auto it = std::lower_bound(cellList.begin(),cellList.end(),query,
                                 [&](const Cell &a, const Cell &b)->bool{return (const Exa::LogicalCell&)a < (const Exa::LogicalCell&)b;});
      DBG(PRINT(level));
      DBG(PRINT(query));
      DBG(PRINT(*it));
      if (it != cellList.end() && (const LogicalCell&)*it == (const LogicalCell&)query) {
        result = it-cellList.begin();
        return true;
      }
    }
    result   = -1;
    return false;
  }


  // ##################################################################
  // managing output vertex and scalar generation
  // ##################################################################

  std::map<vec4f,int> vertexIndex;
  std::mutex vertexMutex;

  std::shared_ptr<UMesh> output;
  std::mutex outputMutex;

  int findOrEmitVertex(const vec4f &v)
  {
    std::lock_guard<std::mutex> lock(vertexMutex);

    auto it = vertexIndex.find(v);
    if (it != vertexIndex.end()) return it->second;
  
    size_t newID = output->vertices.size();
    output->vertices.push_back(vec3f{v.x, v.y, v.z});
    output->perVertex->values.push_back(v.w);
  
    if (newID >= 0x7fffffffull) {
      PING;
      throw std::runtime_error("vertex index overflow ...");
    }
  
    vertexIndex[v] = (int)newID;
    return newID;
  }











  /*! tests if the given four vertices are a plar dual-grid face - note
    this will ONLY wok for (possibly degen) dual cells, it will _NOT_
    do a general planarity test (eg, it would not detect rotations of
    the vertices) */
  template<int U, int V>
  inline bool isPlanarQuadFaceT(const vec4f base00,
                                const vec4f base01,
                                const vec4f base10,
                                const vec4f base11)
  {
    const vec2f v00 = vec2f(base00[U],base00[V]);
    const vec2f v01 = vec2f(base01[U],base01[V]);
    const vec2f v10 = vec2f(base10[U],base10[V]);
    const vec2f v11 = vec2f(base11[U],base11[V]);
    return
      (v00 == v01 && v10 == v11) ||
      (v00 == v10 && v01 == v11);
  }

  /*! tests if the given four vertices are a plar dual-grid face - note
    this will ONLY wok for (possibly degen) dual cells, it will _NOT_
    do a general planarity test (eg, it would not detect rotations of
    the vertices) */
  bool isPlanarQuadFace(const vec4f base00,
                        const vec4f base01,
                        const vec4f base10,
                        const vec4f base11)
  {
    return
      isPlanarQuadFaceT<0,1>(base00,base01,base10,base11) ||
      isPlanarQuadFaceT<0,2>(base00,base01,base10,base11) ||
      isPlanarQuadFaceT<1,2>(base00,base01,base10,base11) ||
      // mirror
      isPlanarQuadFaceT<1,0>(base00,base01,base10,base11) ||
      isPlanarQuadFaceT<2,0>(base00,base01,base10,base11) ||
      isPlanarQuadFaceT<2,1>(base00,base01,base10,base11);
  }




  // ##################################################################
  // actual 'emit' functions - these *will* write the specified prim w/o
  // any other tests
  // ##################################################################

  void printCounts()
  {
    std::cout << "generated "
              << prettyNumber(numTets) << " tets, "
    
              << prettyNumber(numPyramids) << " pyramids ("
              << prettyNumber(numPyramidsPerfect) << " perfect, " 
              << prettyNumber(numPyramidsTwisted) << " twisted), " 

              << prettyNumber(numWedges) << " wedges ("
              << prettyNumber(numWedgesPerfect) << " perfect, " 
              << prettyNumber(numWedgesTwisted) << " twisted), " 

              << prettyNumber(numHexes) << " hexes ("
              << prettyNumber(numHexesPerfect) << " perfect, " 
              << prettyNumber(numHexesTwisted) << " twisted)." 
              << std::endl;
  }

  void emitTet(const vec4f &A,
               const vec4f &B,
               const vec4f &C,
               const vec4f &D)
  {
    const vec4i tet(findOrEmitVertex(A),
                    findOrEmitVertex(B),
                    findOrEmitVertex(C),
                    findOrEmitVertex(D));
  
    assert(tet.x != tet.y);
    assert(tet.x != tet.z);
    assert(tet.x != tet.w);

    assert(tet.y != tet.z);
    assert(tet.y != tet.w);

    assert(tet.z != tet.w);
  
    std::lock_guard<std::mutex> lock(outputMutex);
    output->tets.push_back({(int)tet.x, (int)tet.y, (int)tet.z, (int)tet.w});
  
    static int nextPing = 1;
    if (numTets++ >= nextPing) {
      nextPing*=2;
      printCounts();
    }
  };

  // ##################################################################
  void emitPyramid(const vec4f &top,
                   const vec4f &base00,
                   const vec4f &base01,
                   const vec4f &base10,
                   const vec4f &base11)
  {
    UMesh::Pyr pyr;
    pyr[4]    = findOrEmitVertex(top);
    pyr[0] = findOrEmitVertex(base00);
    pyr[1] = findOrEmitVertex(base01);
    pyr[2] = findOrEmitVertex(base11);
    pyr[3] = findOrEmitVertex(base10);

    if (isPlanarQuadFace(base00,base01,base10,base11))
      numPyramidsPerfect++;
    else
      numPyramidsTwisted++;
  
    std::lock_guard<std::mutex> lock(outputMutex);
    output->pyrs.push_back(pyr);

    static int nextPing = 1;
    if (numPyramids++ >= nextPing) {
      nextPing*=2;
      printCounts();
    }
  }

  void emitWedge(const vec4f &top0,
                 const vec4f &top1,
                 const vec4f &base00,
                 const vec4f &base01,
                 const vec4f &base10,
                 const vec4f &base11)
  {
    UMesh::Wedge wedge;
    wedge[0] = findOrEmitVertex(base00);
    wedge[1] = findOrEmitVertex(base01);
    wedge[2] = findOrEmitVertex(top0);
    wedge[3]  = findOrEmitVertex(base10);
    wedge[4]  = findOrEmitVertex(base11);
    wedge[5]  = findOrEmitVertex(top1);
    // wedge.base[0] = findOrEmitVertex(base00);
    // wedge.base[1] = findOrEmitVertex(base01);
    // wedge.base[2] = findOrEmitVertex(top0);
    // wedge.top[0]  = findOrEmitVertex(base10);
    // wedge.top[1]  = findOrEmitVertex(base11);
    // wedge.top[2]  = findOrEmitVertex(top1);

    if (isPlanarQuadFace(base00,base01,base10,base11) &&
        isPlanarQuadFace(top0,top1,base00,base01) &&
        isPlanarQuadFace(top0,top1,base10,base11))
      numWedgesPerfect++;
    else
      numWedgesTwisted++;
    
    std::lock_guard<std::mutex> lock(outputMutex);
    output->wedges.push_back(wedge);
  
    static int nextPing = 1;
    if (numWedges++ >= nextPing) {
      nextPing*=2;
      printCounts();
    }
  }





  void emitHex(const vec4f corner[2][2][2], bool perfect)
  {
    UMesh::Hex hex;
    hex[0] = findOrEmitVertex(corner[0][0][0]);
    hex[1] = findOrEmitVertex(corner[0][0][1]);
    hex[2] = findOrEmitVertex(corner[0][1][1]);
    hex[3] = findOrEmitVertex(corner[0][1][0]);
  
    hex[4] = findOrEmitVertex(corner[1][0][0]);
    hex[5] = findOrEmitVertex(corner[1][0][1]);
    hex[6] = findOrEmitVertex(corner[1][1][1]);
    hex[7] = findOrEmitVertex(corner[1][1][0]);
  
  
    std::lock_guard<std::mutex> lock(outputMutex);
    output->hexes.push_back(hex);

    if (perfect)
      numHexesPerfect++;
    else
      numHexesTwisted++;
  
    static int nextPing = 1;
    if (numHexes++ >= nextPing) {
      nextPing*=2;
      printCounts();
    }
  }




  /*! check if the given four vertices form a valid tet, and if so, emit
    (else just drop it) */
  void tryTet(const vec4f &A,
              const vec4f &B,
              const vec4f &C,
              const vec4f &D)
  {
    if (A == B) return;
    if (A == C) return;
    if (A == D) return;
    if (B == C) return;
    if (B == D) return;
    if (C == D) return;
    emitTet(A,B,C,D);
  }




  void tryPyramid(const vec4f &top,
                  const vec4f &base00,
                  const vec4f &base01,
                  const vec4f &base10,
                  const vec4f &base11)
  {
    if (base00 == base01) {
      tryTet(top,base00,base10,base11);
      return;
    }
    if (base00 == base10) {
      tryTet(top,base00,base01,base11);
      return;
    }
    if (base11 == base10) {
      tryTet(top,base11,base01,base00);
      return;
    }
    if (base11 == base01) {
      tryTet(top,base11,base10,base00);
      return;
    }

    emitPyramid(top,base00,base01,base10,base11);
  }

  void tryWedge(const vec4f &top0,
                const vec4f &top1,
                const vec4f &base00,
                const vec4f &base01,
                const vec4f &base10,
                const vec4f &base11,
                int numDuplicates)
  {
    if (numDuplicates == 2) {
      emitWedge(top0,top1,base00,base01,base10,base11);
      return;
    }

    // OK, if there's MORE than two duplicates then the only way the
    // bottom face could be a true bilinear patch is if the top was a
    // single point - which it isn't, or it'd have been catched in the
    // pyramid cases. As such, the bottom is either a triangle, or
    // totally degenerate, but etiher way we can tessellate it into tets
    tryTet(top0,top1,base10,base11);
    tryPyramid(top0,base00,base10,base10,base11);
  }









  // ##################################################################
  // logic of processing a dual mesh cell
  // ##################################################################

  bool allSame(const vec4f &a, const vec4f &b, const vec4f &c, const vec4f &d)
  {
    return (a==b) && (a==c) && (a==d);
  }

  bool same(const vec4f &a, const vec4f &b)
  {
    return (a==b);
  }

  void splitHex(const Exa &exa, const vec4f vertex[2][2][2])
  {
    const vec4f &v000 = vertex[0][0][0];
    const vec4f &v001 = vertex[0][0][1];
    const vec4f &v010 = vertex[0][1][0];
    const vec4f &v011 = vertex[0][1][1];
    const vec4f &v100 = vertex[1][0][0];
    const vec4f &v101 = vertex[1][0][1];
    const vec4f &v110 = vertex[1][1][0];
    const vec4f &v111 = vertex[1][1][1];
  
    if (allSame(v000,v001,v010,v011)) {
      tryPyramid(v000, v100,v101,v110,v111);
      return;
    }
    if (allSame(v100,v101,v110,v111)) {
      tryPyramid(v100, v000,v001,v010,v011);
      return;
    }
  
    if (allSame(v000,v001,v100,v101)) {
      tryPyramid(v000, v010,v011,v110,v111);
      return;
    }
    if (allSame(v010,v011,v110,v111)) {
      tryPyramid(v010, v000,v001,v100,v101);
      return;
    }
  
    if (allSame(v000,v010,v100,v110)) {
      tryPyramid(v000, v001,v011,v101,v111);
      return;
    }
    if (allSame(v001,v011,v101,v111)) {
      tryPyramid(v001, v000,v010,v100,v110);
      return;
    }

    vec4f center(0.f);
    for (int iz=0;iz<2;iz++)
      for (int iy=0;iy<2;iy++)
        for (int ix=0;ix<2;ix++) {
          center = center + vertex[iz][iy][ix];
        }
    center = center * vec4f(1.f / 8.f);

    tryPyramid(center, v000, v001, v010, v011);
    tryPyramid(center, v100, v101, v110, v111);

    tryPyramid(center, v000, v001, v100, v101);
    tryPyramid(center, v010, v011, v110, v111);
  
    tryPyramid(center, v000, v010, v100, v110);
    tryPyramid(center, v001, v011, v101, v111);
  }



  // ##################################################################
  // code that actually generates the (possibly-degenerate) dual cells
  // ##################################################################
  void doCell(const Exa &exa, const Exa::Cell &cell)
  {
    int selfID;
    exa.find(selfID,cell.center());
    if (selfID < 0 || exa.cellList[selfID] != cell)
      throw std::runtime_error("bug in exa::find()");
    
    const int cellWidth = (1<<cell.level);
    for (int dz=-1;dz<=1;dz+=2)
      for (int dy=-1;dy<=1;dy+=2)
        for (int dx=-1;dx<=1;dx+=2) {
          int corner[2][2][2];
          int minLevel = 1000;
          int maxLevel = -1;
          int numFound = 0;
          for (int iz=0;iz<2;iz++)
            for (int iy=0;iy<2;iy++)
              for (int ix=0;ix<2;ix++) {
                // PRINT(vec3i(ix,iy,iz));
                const vec3f cornerCenter = cell.neighbor(vec3i(dx*ix,dy*iy,dz*iz)).center();
                // PRINT(cornerCenter);
                if (!exa.find(corner[iz][iy][ix],cornerCenter))
                  // corner does not exist, this is not a dual cell
                  continue;
              
                minLevel = min(minLevel,exa.cellList[corner[iz][iy][ix]].level);
                maxLevel = max(maxLevel,exa.cellList[corner[iz][iy][ix]].level);
                ++numFound;
              }
          if (numFound < 8)
            continue;
          
          if (minLevel < cell.level)
            // somebody else will generate this same cell from a finer
            // level...
            continue;

          Exa::Cell minCell = cell;
          for (int iz=0;iz<2;iz++)
            for (int iy=0;iy<2;iy++)
              for (int ix=0;ix<2;ix++) {
                int cID = corner[iz][iy][ix];
                if (cID < 0) continue;
                Exa::Cell cc = exa.cellList[cID];
                if (cc.level == cell.level && cc < minCell)
                  minCell = cc;
              }
          if (minCell != cell)
            // some other cell will generate this
            continue;

          int numDuplicates = 0;
          int *asList = (int*)&corner[0][0][0];
          for (int i=0;i<8;i++) {
            bool i_is_duplicate = false;
            for (int j=0;j<i;j++)
              if (asList[i] == asList[j])
                i_is_duplicate = true;
            if (i_is_duplicate)
              numDuplicates++;
          }

          vec4f vertex[2][2][2];
          for (int iz=0;iz<2;iz++)
            for (int iy=0;iy<2;iy++)
              for (int ix=0;ix<2;ix++) {
                const Exa::Cell &c = exa.cellList[corner[iz][iy][ix]];
                vertex[iz][iy][ix] = vec4f(c.center(),c.scalar);
              }

          const vec4f &v000 = vertex[0][0][0];
          const vec4f &v001 = vertex[0][0][1];
          const vec4f &v010 = vertex[0][1][0];
          const vec4f &v011 = vertex[0][1][1];
          const vec4f &v100 = vertex[1][0][0];
          const vec4f &v101 = vertex[1][0][1];
          const vec4f &v110 = vertex[1][1][0];
          const vec4f &v111 = vertex[1][1][1];
        

          // ==================================================================
          // check for regular cube
          // ==================================================================
          if (minLevel == maxLevel) {
            emitHex(vertex,/*perfect:*/true);
            return;
          }

          // ==================================================================
          // check for general hex (with possibly twisted sides)
          // ==================================================================
          if (numDuplicates == 0) {
            emitHex(vertex,/*perfect:*/false);
            return;
          }
        
          // ==================================================================
          // check whether an entire face completely collapsed - then
          // it's a pyramid, a tet, or degenerate
          // ==================================================================
          if (allSame(v000,v001,v010,v011)) {
            tryPyramid(v000, v100,v101,v110,v111);
            return;
          }
          if (allSame(v100,v101,v110,v111)) {
            tryPyramid(v100, v000,v001,v010,v011);
            return;
          }
  
          if (allSame(v000,v001,v100,v101)) {
            tryPyramid(v000, v010,v011,v110,v111);
            return;
          }
          if (allSame(v010,v011,v110,v111)) {
            tryPyramid(v010, v000,v001,v100,v101);
            return;
          }
  
          if (allSame(v000,v010,v100,v110)) {
            tryPyramid(v000, v001,v011,v101,v111);
            return;
          }
          if (allSame(v001,v011,v101,v111)) {
            tryPyramid(v001, v000,v010,v100,v110);
            return;
          }


          // ==================================================================
          // no face that completely collapsed - check if there's any
          // that collapsed to an edge - then it's either a wedge, a
          // tet, or degenerate
          // ==================================================================
        
          if (same(v000,v001) && same(v010,v011)) {
            tryWedge(v000,v010, v100,v110,v101,v111,numDuplicates);
            return;
          }
          if (same(v100,v101) && same(v110,v111)) {
            tryWedge(v100,v110, v000,v010,v001,v011,numDuplicates);
            return;
          }

          if (same(v000,v010) && same(v001,v011)) {
            tryWedge(v000,v001, v100,v101,v110,v111,numDuplicates);
            return;
          }
          if (same(v100,v110) && same(v101,v111)) {
            tryWedge(v100,v101, v000,v001,v010,v011,numDuplicates);
            return;
          }

          if (same(v000,v100) && same(v001,v101)) {
            tryWedge(v000,v001, v010,v011,v110,v111,numDuplicates);
            return;
          }
          if (same(v010,v110) && same(v011,v111)) {
            tryWedge(v010,v011, v000,v001,v100,v101,numDuplicates);
            return;
          }

          if (same(v010,v011) && same(v110,v111)) {
            tryWedge(v010,v011, v000,v100,v001,v101,numDuplicates);
            return;
          }
          if (same(v000,v001) && same(v100,v101)) {
            tryWedge(v000,v001, v010,v110,v011,v111,numDuplicates);
            return;
          }

          if (same(v001,v011) && same(v101,v111)) {
            tryWedge(v001,v101, v000,v100,v010,v110,numDuplicates);
            return;
          }
          if (same(v000,v010) && same(v100,v110)) {
            tryWedge(v000,v100, v001,v101,v011,v111,numDuplicates);
            return;
          }


          if (same(v001,v101) && same(v011,v111)) {
            tryWedge(v001,v011, v000,v010,v100,v110,numDuplicates);
            return;
          }
          if (same(v000,v100) && same(v010,v110)) {
            tryWedge(v000,v010, v001,v011,v101,v111,numDuplicates);
            return;
          }


          // ==================================================================
          // fallback - there's still cases of only ONE collapsed vertex,
          // for example, so let's just make this into a deformed hex 
          // ==================================================================
          emitHex(vertex,/*perfect:*/false);
          return;
        }
  }

  
  void process(Exa &exa)
  {
    std::cout << "sorting cell list for query" << std::endl;
    std::sort(exa.cellList.begin(),exa.cellList.end());
    std::cout << "Sorted .... starting to query" << std::endl;
#if DEBUG
    serial_for
#else
      parallel_for
#endif
      (exa.cellList.size(),
       [&](size_t cellID){
         // PING; PRINT(cellID);
         const Exa::Cell &cell = exa.cellList[cellID];
         doCell(exa,cell);
       });
  }




  extern "C" int main(int ac, char **av)
  {
    if (ac != 4 && ac != 6)
      throw std::runtime_error("./exa2umesh in.cells scalar.scalars out.umesh\n");
    cout.precision(10);
    Exa exa;
    std::ifstream in_cells(av[1]);
    std::ifstream in_scalars(av[2]);
    output = std::make_shared<UMesh>();
    std::string outFileName = av[3];
  
    while (!in_cells.eof()) {
      Exa::Cell cell;
      in_cells.read((char*)&cell,sizeof(Exa::LogicalCell));
      in_scalars.read((char*)&cell.scalar,sizeof(float));
    
      if (!(in_cells.good() && in_scalars.good()))
        break;
    
      exa.add(cell);
      static size_t nextPing = 1;
      if (exa.size() >= nextPing) {
        std::cout << "read so far : " << exa.size() << ", bounds " << exa.bounds << std::endl;
        nextPing *= 2;
      }
    }
    std::cout << "done reading, found " << prettyNumber(exa.size()) << " cells" << std::endl;

    output->perVertex = std::make_shared<Attribute>();
    
    process(exa);

    output->finalize();
    //io::saveBinaryUMesh(outFileName,output);
    output->saveTo(outFileName);
  }

}
