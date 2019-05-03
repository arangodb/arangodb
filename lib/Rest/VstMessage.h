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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_VST_MESSAGE_H
#define ARANGODB_REST_VST_MESSAGE_H 1

#include <velocypack/Buffer.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>
#include <iterator>
#include <vector>
#include "Basics/Common.h"
#include "Basics/VelocyPackHelper.h"

namespace arangodb {
namespace rest {

struct VstInputMessage {
  VstInputMessage() : _id(0), _buffer() {}

  // no copy
  VstInputMessage(VstInputMessage const& other) = delete;

  // just move
  VstInputMessage(VstInputMessage&& other)
      : _id(other._id), _buffer(std::move(other._buffer)) {}

  void set(uint64_t id, VPackBuffer<uint8_t>&& buff) {
    _id = id;
    _buffer = buff;
  }

  /// @brief message header, validated
  velocypack::Slice header() const {
    if (_buffer.size() > 0) {
      return VPackSlice(_buffer.data());
    }
    return velocypack::Slice::emptyArraySlice();
  };
  /// message payload, not validated
  arangodb::velocypack::StringRef payload() const {
    size_t len = velocypack::Slice(_buffer.data()).byteSize();
    if (_buffer.size() > len) {
      return arangodb::velocypack::StringRef(reinterpret_cast<const char*>(_buffer.data() + len),
                                 _buffer.size() - len);
    }
    return arangodb::velocypack::StringRef();
  }
  size_t payloadSize() const {
    size_t len = velocypack::Slice(_buffer.data()).byteSize();
    return _buffer.size() - len;
  }

 private:
  uint64_t _id;  // id zero signals invalid state
  velocypack::Buffer<uint8_t> _buffer;
};

struct VPackMessageNoOwnBuffer {
  // cppcheck-suppress *
  VPackMessageNoOwnBuffer(velocypack::Slice head, std::vector<velocypack::Slice>&& payloads,
                          uint64_t id, bool generateBody = true)
      : _header(head), _payloads(std::move(payloads)), _id(id), _generateBody(generateBody) {}

  VPackMessageNoOwnBuffer(velocypack::Slice head, velocypack::Slice payload,
                          uint64_t id, bool generateBody = true)
      : _header(head), _payloads(), _id(id), _generateBody(generateBody) {
    _payloads.push_back(payload);
  }

  velocypack::Slice firstPayload() {
    if (_payloads.size() && _generateBody) {
      return _payloads.front();
    }
    return velocypack::Slice::noneSlice();
  }

  std::vector<velocypack::Slice> payloads() { return _payloads; }

  velocypack::Slice _header;
  std::vector<velocypack::Slice> _payloads;
  uint64_t _id;
  bool _generateBody;
};
}  // namespace rest
}  // namespace arangodb
#endif
