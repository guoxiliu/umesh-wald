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

// ours:
#include "UMesh.h"
#include "io/UMesh.h"
#include "io/IO.h"
#include <sstream>

namespace umesh {
  
  const size_t bum_magic = 0x234235566ULL;
  
  /*! helper functoin for printf debugging - puts the four elemnt
    sizes into a human-readable (short) string*/
  std::string sizeString(UMesh::SP mesh)
  {
    std::stringstream str;
    str << "v:" << prettyNumber(mesh->vertices.size())
        << ",t:" << prettyNumber(mesh->tets.size())
        << ",p:" << prettyNumber(mesh->pyrs.size())
        << ",w:" << prettyNumber(mesh->wedges.size())
        << ",h:" << prettyNumber(mesh->hexes.size());
    return str.str();
  }
  

  /*! write - binary - to given (bianry) stream */
  void UMesh::writeTo(std::ostream &out) const
  {
    io::writeElement(out,bum_magic);
    io::writeVector(out,vertices);
    // PRINT(perVertex->values[0]);
    // PRINT(perVertex->values.back());
    if (perVertex) {
      io::writeVector(out,perVertex->values);
    } else {
      std::vector<float> dummy;
      io::writeVector(out,dummy);
    }
    io::writeVector(out,triangles);
    io::writeVector(out,quads);
    io::writeVector(out,tets);
    io::writeVector(out,pyrs);
    io::writeVector(out,wedges);
    io::writeVector(out,hexes);
    io::writeVector(out,vertexTag);
  }
  
  /*! write - binary - to given file */
  void UMesh::saveTo(const std::string &fileName) const
  {
    std::ofstream out(fileName, std::ios_base::binary);
    writeTo(out);
  }
    
  /*! read from given (binary) stream */
  void UMesh::readFrom(std::istream &in)
  {
    size_t magic;
    io::readElement(in,magic);
    if (magic != bum_magic)
      throw std::runtime_error("wrong magic number in umesh file ...");
    io::readVector(in,this->vertices,"vertices");

    std::vector<float> scalars;
    io::readVector(in,scalars,"scalars");
    if (!scalars.empty()) {
      this->perVertex = std::make_shared<Attribute>();
      this->perVertex->values = scalars;
      this->perVertex->finalize();
    }
      
    io::readVector(in,this->triangles,"triangles");
    io::readVector(in,this->quads,"quads");
    io::readVector(in,this->tets,"tets");
    io::readVector(in,this->pyrs,"pyramids");
    io::readVector(in,this->wedges,"wedges");
    io::readVector(in,this->hexes,"hexes");
    // try {
    if (!in.eof())
      try {
        io::readVector(in,this->vertexTag,"vertexTags");
      } catch (...) {
        /* ignore ... */
      }
  
    this->finalize();
  }

  /*! read from given file, assuming file format as used by saveTo() */
  UMesh::SP UMesh::loadFrom(const std::string &fileName)
  {
    UMesh::SP mesh = std::make_shared<UMesh>();
    std::ifstream in(fileName, std::ios_base::binary);
    mesh->readFrom(in);
    return mesh;
  }
    

  /*! tells this attribute that its values are set, and precomputations can be done */
  void Attribute::finalize()
  {

#if 1
    std::mutex mutex;
    parallel_for_blocked(0,values.size(),16*1024,[&](size_t begin, size_t end) {
                                                   range1f rangeValueRange;
                                                   for (size_t i=begin;i<end;i++)
                                                     rangeValueRange.extend(values[i]);
                                                   std::lock_guard<std::mutex> lock(mutex);
                                                   valueRange.extend(rangeValueRange.lower);
                                                   valueRange.extend(rangeValueRange.upper);
                                                 });
#else
    for (auto &v : value) valueRange.extend(v);
    // for (auto v : vertex) bounds.extend(v);
#endif
  }
  
  /*! if data set has per-vertex data this won't change anything; if
    it has per-cell scalar values it will compute create a new
    per-vertex data field based on the averages of all adjacent
    cell scalars */
  void UMesh::createPerVertexData()
  {
    std::cout << "=======================================================" << std::endl;
    
    if (perVertex) return;

    if (!perTet && !perHex)
      throw std::runtime_error("cannot generate per-vertex interpolated scalar field: data set has neither per-cell nor per-vertex data fields assigned!?");

    this->perVertex = std::make_shared<Attribute>(vertices.size());

    struct Helper {
      Helper(Attribute::SP perVertex)
        : perVertex(perVertex),
          renormalize_weights(perVertex->values.size())
      {
        assert(perVertex);
      }
      ~Helper()
      {
        for (int i=0;i<renormalize_weights.size();i++)
          if (renormalize_weights[i] > 0)
            perVertex->values[i] *= 1.f/renormalize_weights[i];
        perVertex->finalize();
        PRINT(perVertex->valueRange);
      }
      
      void splat(float value, int vertexID) {
        perVertex->values[vertexID] += value;
        renormalize_weights[vertexID] += 1.f;
      }
      Attribute::SP perVertex;
      std::vector<float> renormalize_weights;
    } helper(this->perVertex);

    assert(pyrs.empty()); // not implemented
    assert(wedges.empty()); // not implemented
    for (int i=0;i<tets.size();i++) {
      helper.splat(perTet->values[i],tets[i].x);
      helper.splat(perTet->values[i],tets[i].y);
      helper.splat(perTet->values[i],tets[i].z);
      helper.splat(perTet->values[i],tets[i].w);
    }
    for (int i=0;i<hexes.size();i++) {
      helper.splat(perHex->values[i],hexes[i].base.x);
      helper.splat(perHex->values[i],hexes[i].base.y);
      helper.splat(perHex->values[i],hexes[i].base.z);
      helper.splat(perHex->values[i],hexes[i].base.w);
      helper.splat(perHex->values[i],hexes[i].top.x);
      helper.splat(perHex->values[i],hexes[i].top.y);
      helper.splat(perHex->values[i],hexes[i].top.z);
      helper.splat(perHex->values[i],hexes[i].top.w);
    }

    // aaaaand .... free the old per-cell data
    this->perTet = nullptr;
    this->perHex = nullptr;
  }
    

  /*! create std::vector of primitmive references (bounding box plus
    tag) for every volumetric prim in this mesh */
  void UMesh::createVolumePrimRefs(std::vector<UMesh::PrimRef> &result)
  {
    result.resize(tets.size()+pyrs.size()+wedges.size()+hexes.size());
    parallel_for_blocked
      (0ull,tets.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[i] = PrimRef(TET,i);
       });
    parallel_for_blocked
      (0ull,pyrs.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[tets.size()+i] = PrimRef(PYR,i);
       });
    parallel_for_blocked
      (0ull,wedges.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[tets.size()+pyrs.size()+i] = PrimRef(WEDGE,i);
       });
    parallel_for_blocked
      (0ull,hexes.size(),64*1024,
       [&](size_t begin,size_t end){
         for (size_t i=begin;i<end;i++)
           result[tets.size()+pyrs.size()+wedges.size()+i] = PrimRef(HEX,i);
       });
  }


  /*! finalize a mesh, and compute min/max ranges where required */
  void UMesh::finalize()
  {
    if (perVertex) perVertex->finalize();
    if (perHex)    perHex->finalize();
    if (perTet)    perTet->finalize();
    bounds = box3f();
    std::mutex mutex;
    parallel_for_blocked(0,vertices.size(),16*1024,[&](size_t begin, size_t end) {
                                                     box3f rangeBounds;
                                                     for (size_t i=begin;i<end;i++)
                                                       rangeBounds.extend(vertices[i]);
                                                     std::lock_guard<std::mutex> lock(mutex);
                                                     bounds.extend(rangeBounds);
                                                   });
  }
  
  /*! print some basic info of this mesh to std::cout */
  void UMesh::print()
  {
    std::cout << toString(false);
  }
  
  /*! return a string of the form "UMesh{#tris=...}" */
  std::string UMesh::toString(bool compact) const
  {
    std::stringstream ss;

    if (compact) {
      ss << "Umesh(";
      ss << "#verts=" << prettyNumber(vertices.size());
      if (!triangles.empty())
        ss << ",#tris=" << prettyNumber(triangles.size());
      if (!quads.empty())
        ss << ",#quads=" << prettyNumber(quads.size());
      if (!tets.empty())
        ss << ",#tets=" << prettyNumber(tets.size());
      if (!pyrs.empty())
        ss << ",#pyrs=" << prettyNumber(pyrs.size());
      if (!wedges.empty())
        ss << ",#wedges=" << prettyNumber(wedges.size());
      if (!hexes.empty())
        ss << ",#hexes=" << prettyNumber(hexes.size());
      if (perVertex) {
        ss << ",scalars=yes(name='" << perVertex->name << "')";
      } else {
        ss << ",scalars=no";
      }
      ss << ")";
    } else {
      ss << "#verts : " << prettyNumber(vertices.size()) << std::endl;
      ss << "#tris  : " << prettyNumber(triangles.size()) << std::endl;
      ss << "#quads : " << prettyNumber(quads.size()) << std::endl;
      ss << "#tets  : " << prettyNumber(tets.size()) << std::endl;
      ss << "#pyrs  : " << prettyNumber(pyrs.size()) << std::endl;
      ss << "#wedges: " << prettyNumber(wedges.size()) << std::endl;
      ss << "#hexes : " << prettyNumber(hexes.size()) << std::endl;
      ss << "bounds : " << bounds << std::endl;
      if (perVertex)
        ss << "values : " << getValueRange() << std::endl;
      else
        ss << "values : <none>" << std::endl;
    }
    return ss.str();
  }

} // ::tetty
