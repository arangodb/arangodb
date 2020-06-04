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

#ifndef VELOCYPACK_DUMPER_H
#define VELOCYPACK_DUMPER_H 1

#include <string>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"
#include "velocypack/Options.h"
#include "velocypack/Sink.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace velocypack {

// Dumps VPack into a JSON output string
class Dumper {
 public:
  Options const* options;

  Dumper(Dumper const&) = delete;
  Dumper& operator=(Dumper const&) = delete;

  explicit Dumper(Sink* sink, Options const* options = &Options::Defaults)
      : options(options), _sink(sink), _indentation(0) {
    if (VELOCYPACK_UNLIKELY(sink == nullptr)) {
      throw Exception(Exception::InternalError, "Sink cannot be a nullptr");
    }
    if (VELOCYPACK_UNLIKELY(options == nullptr)) {
      throw Exception(Exception::InternalError, "Options cannot be a nullptr");
    }
  }

  ~Dumper() {}

  Sink* sink() const { return _sink; }

  void dump(Slice const& slice) {
    _indentation = 0;
    _sink->reserve(slice.byteSize());
    dumpValue(&slice);
  }

  void dump(Slice const* slice) { dump(*slice); }

  static void dump(Slice const& slice, Sink* sink,
                   Options const* options = &Options::Defaults) {
    Dumper dumper(sink, options);
    dumper.dump(slice);
  }

  static void dump(Slice const* slice, Sink* sink,
                   Options const* options = &Options::Defaults) {
    dump(*slice, sink, options);
  }

  static std::string toString(Slice const& slice,
                              Options const* options = &Options::Defaults) {
    std::string buffer;
    StringSink sink(&buffer);
    dump(slice, &sink, options);
    return buffer;
  }

  static std::string toString(Slice const* slice,
                              Options const* options = &Options::Defaults) {
    return toString(*slice, options);
  }

  void append(Slice const& slice) { dumpValue(&slice); }

  void append(Slice const* slice) { dumpValue(slice); }

  void appendString(char const* src, ValueLength len) {
    _sink->reserve(2 + len);
    _sink->push_back('"');
    dumpString(src, len);
    _sink->push_back('"');
  }

  void appendString(std::string const& str) {
    _sink->reserve(2 + str.size());
    _sink->push_back('"');
    dumpString(str.data(), str.size());
    _sink->push_back('"');
  }

  void appendUInt(uint64_t);

  void appendInt(int64_t);

  void appendDouble(double);

 private:
  void dumpUnicodeCharacter(uint16_t value);

  void dumpInteger(Slice const*);

  void dumpString(char const*, ValueLength);

  inline void dumpValue(Slice const& slice, Slice const* base = nullptr) {
    dumpValue(&slice, base);
  }

  void dumpValue(Slice const*, Slice const* = nullptr);

  void indent() {
    std::size_t n = _indentation;
    _sink->reserve(2 * n);
    for (std::size_t i = 0; i < n; ++i) {
      _sink->append("  ", 2);
    }
  }

  void handleUnsupportedType(Slice const* slice) {
    if (options->unsupportedTypeBehavior == Options::NullifyUnsupportedType) {
      _sink->append("null", 4);
      return;
    } else if (options->unsupportedTypeBehavior == Options::ConvertUnsupportedType) {
      _sink->append(std::string("\"(non-representable type ") + slice->typeName() + ")\"");
      return;
    }

    throw Exception(Exception::NoJsonEquivalent);
  }

 private:
  Sink* _sink;

  int _indentation;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
