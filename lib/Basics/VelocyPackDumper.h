////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_VELOCY_PACK_DUMPER_H
#define ARANGODB_BASICS_VELOCY_PACK_DUMPER_H 1

#include "Basics/Common.h"
#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"

#include <velocypack/Exception.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace velocypack {
struct Options;
}

namespace basics {

class VelocyPackDumper {
 public:
  VelocyPackDumper(VelocyPackDumper const&) = delete;
  VelocyPackDumper& operator=(VelocyPackDumper const&) = delete;

 public:
  VelocyPackDumper(StringBuffer* buffer, velocypack::Options const* options)
      : options(options), _buffer(buffer) {
    TRI_ASSERT(buffer != nullptr);
    TRI_ASSERT(options != nullptr);
  }

  ~VelocyPackDumper() = default;

  void dump(velocypack::Slice const& slice) {
    _buffer->reserve(slice.byteSize());
    dumpValue(&slice);
  }

  void dump(velocypack::Slice const* slice) { dump(*slice); }

  void append(velocypack::Slice const& slice) { dumpValue(&slice); }

  void append(velocypack::Slice const* slice) { dumpValue(slice); }

  void appendUInt(uint64_t);

  void appendDouble(double);

 private:
  void handleUnsupportedType(velocypack::Slice const* slice);

  void dumpInteger(velocypack::Slice const*);

  void dumpString(char const*, velocypack::ValueLength);

  inline void dumpValue(velocypack::Slice const& slice, velocypack::Slice const* base = nullptr) {
    dumpValue(&slice, base);
  }

  void dumpValue(velocypack::Slice const*, velocypack::Slice const* = nullptr);

 public:
  velocypack::Options const* options;

 private:
  StringBuffer* _buffer;
};

}
}

#endif
