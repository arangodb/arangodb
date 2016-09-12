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

#ifndef ARANGODB_REST_VPP_MESSAGE_H
#define ARANGODB_REST_VPP_MESSAGE_H 1

#include <velocypack/Buffer.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <iterator>
#include <vector>
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"

namespace arangodb {
namespace rest {

struct VppInputMessage {
  VppInputMessage() : _buffer(), _id(0), _payloadAmount(0), _payload() {}

  // cppcheck-suppress *
  VppInputMessage(uint64_t id, VPackBuffer<uint8_t>&& buff,
                  std::size_t amount = 1)
      : _buffer(std::move(buff)), _id(id), _payloadAmount(amount) {
    init();
  }

  // no copy
  VppInputMessage(VppInputMessage const& other) = delete;

  // just move
  VppInputMessage(VppInputMessage&& other) {
    if (this == &other) {
      return;
    }
    _buffer = std::move(other._buffer);
    _id = other._id;
    _payloadAmount = other._payloadAmount;
    init();
  }

  void set(uint64_t id, VPackBuffer<uint8_t>&& buff, std::size_t amount = 1) {
    _id = id;
    _buffer = buff;
    _payloadAmount = amount;
    init();
  }

  VPackSlice header() const { return _header; };
  VPackSlice payload() const {
    if (!_payload.empty()) {
      return _payload.front();
    }
    return VPackSlice::noneSlice();
  }

  std::vector<VPackSlice> const& payloads() const { return _payload; }

 private:
  VPackBuffer<uint8_t> _buffer;
  uint64_t _id;  // id zero signals invalid state
  std::size_t _payloadAmount;
  VPackSlice _header;
  std::vector<VPackSlice> _payload;

  void init() {
    _header = VPackSlice(_buffer.data());
    std::size_t offset = _header.byteSize();

    for (std::size_t sliceNum = 0; sliceNum < _payloadAmount; ++sliceNum) {
      _payload.emplace_back(_buffer.data() + offset);
      offset += _payload.back().byteSize();
    }
  }
};

struct VPackMessageNoOwnBuffer {
  // cppcheck-suppress *
  VPackMessageNoOwnBuffer(VPackSlice head, std::vector<VPackSlice>&& payloads,
                          uint64_t id, bool generateBody = true)
      : _header(head),
        _payloads(std::move(payloads)),
        _id(id),
        _generateBody(generateBody) {}

  VPackMessageNoOwnBuffer(VPackSlice head, VPackSlice payload, uint64_t id,
                          bool generateBody = true)
      : _header(head), _payloads(), _id(id), _generateBody(generateBody) {
    _payloads.push_back(payload);
  }

  VPackSlice firstPayload() {
    if (_payloads.size() && _generateBody) {
      return _payloads.front();
    }
    return VPackSlice::noneSlice();
  }

  std::vector<VPackSlice> payloads() { return _payloads; }

  VPackSlice _header;
  std::vector<VPackSlice> _payloads;
  uint64_t _id;
  bool _generateBody;
};
}
}
#endif
