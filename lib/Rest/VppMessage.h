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

#include "Basics/StringBuffer.h"
#include <velocypack/Options.h>
#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace rest {

struct VPackMessage {
  VPackMessage()
      : _buffer(), _header(), _payload(), _id(), _generateBody(true) {}
  VPackMessage(VPackBuffer<uint8_t>&& buff, VPackSlice head, VPackSlice pay,
               uint64_t id, bool generateBody = true)
      : _buffer(std::move(buff)),
        _header(head),
        _payload(pay),
        _id(id),
        _generateBody(generateBody) {}
  VPackMessage(VPackMessage&& other) = default;

  VPackBuffer<uint8_t> _buffer;
  VPackSlice _header;
  VPackSlice _payload;
  uint64_t _id;
  bool _generateBody;
};

struct VPackMessageNoOwnBuffer : VPackMessage {
  VPackMessageNoOwnBuffer(VPackSlice head, VPackSlice pay, uint64_t id,
                          bool generateBody = true)
      : VPackMessage(VPackBuffer<uint8_t>(), head, pay, id, generateBody) {}
};
}
}
#endif
