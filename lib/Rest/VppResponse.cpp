///////////////////////////////////////////////////////////////////////////////
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "VppResponse.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackDumper.h"
#include "Basics/tri-strings.h"
#include "Meta/conversion.h"
#include "Rest/VppRequest.h"

using namespace arangodb;
using namespace arangodb::basics;

bool VppResponse::HIDE_PRODUCT_HEADER = false;

VppResponse::VppResponse(ResponseCode code, uint64_t id)
    : GeneralResponse(code), _header(nullptr), _messageId(id) {
  _contentType = ContentType::VPACK;
  _connectionType = rest::CONNECTION_KEEP_ALIVE;
}

void VppResponse::reset(ResponseCode code) {
  _responseCode = code;
  _headers.clear();
  _connectionType = rest::CONNECTION_KEEP_ALIVE;
  _contentType = ContentType::VPACK;
  _generateBody = false;  // payload has to be set
}

VPackMessageNoOwnBuffer VppResponse::prepareForNetwork() {
  // initalize builder with vpackbuffer. then we do not need to
  // steal the header and can avoid the shared pointer
  VPackBuilder builder;
  builder.openArray();
  builder.add(VPackValue(int(1)));
  builder.add(VPackValue(int(2)));  // 2 == response
  builder.add(
      VPackValue(static_cast<int>(meta::underlyingValue(_responseCode))));
  builder.close();
  _header = builder.steal();
  if (_vpackPayloads.empty()) {
    if (_generateBody) {
      LOG_TOPIC(INFO, Logger::REQUESTS)
          << "Response should generate body but no Data available";
      _generateBody = false;  // no body availalbe
    }
    return VPackMessageNoOwnBuffer(VPackSlice(_header->data()),
                                   VPackSlice::noneSlice(), _messageId,
                                   _generateBody);
  } else {
    std::vector<VPackSlice> slices;
    for (auto const& buffer : _vpackPayloads) {
      slices.emplace_back(buffer.data());
    }
    return VPackMessageNoOwnBuffer(VPackSlice(_header->data()),
                                   std::move(slices), _messageId,
                                   _generateBody);
  }
}
