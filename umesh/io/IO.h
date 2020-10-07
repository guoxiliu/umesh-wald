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

#include "umesh/UMesh.h"
// std
#include <fstream>

namespace umesh {
  namespace io {

    template<typename T>
    inline void readElement(std::istream &in, T &t)
    {
      assert(in.good());
      in.read((char*)&t,sizeof(t));
      if (!in.good())
        throw std::runtime_error("partial read");
    }

    template<typename T>
    inline void readArray(std::istream &in, T *t, size_t N)
    {
      assert(in.good());
      size_t numBytes = N*sizeof(T);
      in.read((char*)t,numBytes);
      if (!in.good())
        throw std::runtime_error("partial read");
    }

    template<typename T>
    inline void readVector(std::istream &in,
                           std::vector<T> &t,
                           const std::string &description="<no description>")
    {
      size_t N;
      readElement(in,N);
      t.resize(N);
      for (size_t i=0;i<N;i++)
        readElement(in,t[i]);
    }

    template<typename T>
    void writeElement(std::ostream &out, const T &t)
    {
      out.write((char*)&t,sizeof(t));
      assert(out.good());
    }

    template<typename T>
    void writeData(std::ostream &out, const T *t, size_t N)
    {
      for (size_t i=0;i<N;i++)
        write(out,t[i]);
      assert(out.good());
    }
    
    template<typename T>
    void writeVector(std::ostream &out, const std::vector<T> &vt)
    {
      size_t N = vt.size();
      writeElement(out,N);
      for (auto &v : vt)
        writeElement(out,v);
      assert(out.good());
    }

    template<typename T>
    inline T readElement(std::istream &in)
    {
      T t;
      io::readElement(in,t);
      return t;
    }
    
    
    struct Exception : public std::exception {
    };

    struct CouldNotOpenException : public io::Exception {
      CouldNotOpenException(const std::string &fileName)
        : fileName(fileName)
      {}

      const std::string fileName;
    };

    struct ReadErrorException : public io::Exception {
    };

    struct WriteErrorException : public io::Exception {
    };

  } // ::tetty::io
} // ::tetty
