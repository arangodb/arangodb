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
#include "Rest/VppRequest.h"

using namespace arangodb;
using namespace arangodb::basics;

bool VppResponse::HIDE_PRODUCT_HEADER = false;

VppResponse::VppResponse(ResponseCode code, uint64_t id)
    : GeneralResponse(code),
      _connectionType(CONNECTION_KEEP_ALIVE),
      _contentType(ContentType::VPACK),
      _header(nullptr),
      _payload(),
      _messageID(id) {}

void VppResponse::reset(ResponseCode code) {
  _responseCode = code;
  _headers.clear();
  _connectionType = CONNECTION_KEEP_ALIVE;
  _contentType = ContentType::TEXT;
}

void VppResponse::setPayload(ContentType contentType,
                             arangodb::velocypack::Slice const& slice,
                             bool generateBody, VPackOptions const& options) {
  if (generateBody) {
    // addPayload(slice);
    if (slice.isEmptyObject()) {
      throw std::logic_error("payload should be empty!!");
    }
    _payload.append(slice.startAs<char>(),
                    std::distance(slice.begin(), slice.end()));
  }
};

VPackMessageNoOwnBuffer VppResponse::prepareForNetwork() {
  VPackBuilder builder;
  builder.openObject();
  for (auto const& item : _headers) {
    builder.add(item.first, VPackValue(item.second));
  }
  builder.close();
  _header = builder.steal();
  return VPackMessageNoOwnBuffer(VPackSlice(_header->data()),
                                 VPackSlice(_payload.data()), _messageID);
}
// void VppResponse::writeHeader(basics::StringBuffer*) {}
