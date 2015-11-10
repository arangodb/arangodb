////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_SINK_H
#define VELOCYPACK_SINK_H 1

#include <string>

#include "velocypack/velocypack-common.h"
#include "velocypack/Buffer.h"

namespace arangodb {
  namespace velocypack {

    struct Sink {
      Sink () {
      }
      Sink (Sink const&) = delete;
      Sink& operator= (Sink const&) = delete;

      virtual ~Sink () {
      }
      virtual void push_back (char c) = 0;
      virtual void append (std::string const& p) = 0;
      virtual void append (char c) = 0;
      virtual void append (char const* p) = 0;
      virtual void append (char const* p, ValueLength len) = 0;
      virtual void append (uint8_t const* p, ValueLength len) = 0;
      virtual void reserve (ValueLength len) = 0;
    };
 
    template<typename T>
    struct ByteBufferSink final : public Sink {
      ByteBufferSink ()
        : buffer() {
      }

      ByteBufferSink (ValueLength size)
        : buffer(size) {
      }
      
      void push_back (char c) override final {
        buffer.push_back(c);
      }

      void append (std::string const& p) override final {
        buffer.append(p.c_str(), p.size());
      }

      void append (char c) override final {
        buffer.push_back(c);
      }

      void append (char const* p) override final {
        buffer.append(p, strlen(p));
      }

      void append (char const* p, ValueLength len) override final {
        buffer.append(p, len);
      }

      void append (uint8_t const* p, ValueLength len) override final {
        buffer.append(p, len);
      }

      void reserve (ValueLength len) override final {
        buffer.reserve(len);
      }

      Buffer<T> buffer;
    };

    struct StringSink final : public Sink {
      StringSink () 
        : buffer() {
      }

      void push_back (char c) override final {
        buffer.push_back(c);
      }

      void append (std::string const& p) override final {
        buffer.append(p);
      }

      void append (char c) override final {
        buffer.push_back(c);
      }

      void append (char const* p) override final {
        buffer.append(p, strlen(p));
      }

      void append (char const* p, ValueLength len) override final {
        buffer.append(p, len);
      }

      void append (uint8_t const* p, ValueLength len) override final {
        buffer.append(reinterpret_cast<char const*>(p), len);
      }

      void reserve (ValueLength len) override final {
        buffer.reserve(len);
      }

      std::string buffer;
    };

    typedef ByteBufferSink<char> CharBufferSink;

  }  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
