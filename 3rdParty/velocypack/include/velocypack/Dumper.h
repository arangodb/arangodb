////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_DUMPER_H
#define VELOCYPACK_DUMPER_H 1

#include <string>

#include "velocypack/velocypack-common.h"
#include "velocypack/Options.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace velocypack {
struct Sink;

// Dumps VPack into a JSON output string
class Dumper {
 public:
  Options const* options;

  Dumper(Dumper const&) = delete;
  Dumper& operator=(Dumper const&) = delete;

  explicit Dumper(Sink* sink, Options const* options = &Options::Defaults);

  ~Dumper() = default;

  Sink* sink() const { return _sink; }

  void dump(Slice const& slice);

  void dump(Slice const* slice) { dump(*slice); }

  static void dump(Slice const& slice, Sink* sink,
                   Options const* options = &Options::Defaults);

  static void dump(Slice const* slice, Sink* sink,
                   Options const* options = &Options::Defaults);

  static std::string toString(Slice const& slice,
                              Options const* options = &Options::Defaults);

  static std::string toString(Slice const* slice,
                              Options const* options = &Options::Defaults);

  void append(Slice const& slice) { dumpValue(&slice); }

  void append(Slice const* slice) { dumpValue(slice); }

  void appendString(char const* src, ValueLength len);

  void appendString(std::string const& str) {
    appendString(str.data(), str.size());
  }

  void appendInt(int64_t);
  
  void appendUInt(uint64_t);

  void appendDouble(double);

 private:
  void dumpUnicodeCharacter(uint16_t value);

  void dumpInteger(Slice const*);

  void dumpString(char const*, ValueLength);

  inline void dumpValue(Slice const& slice, Slice const* base = nullptr) {
    dumpValue(&slice, base);
  }

  void dumpValue(Slice const*, Slice const* = nullptr);

  void indent();

  void handleUnsupportedType(Slice const* slice);

 private:
  Sink* _sink;

  int _indentation;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
