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

#include "VstResponse.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/tri-strings.h"
#include "Meta/conversion.h"
#include "Rest/VstRequest.h"

using namespace arangodb;
using namespace arangodb::basics;

bool VstResponse::HIDE_PRODUCT_HEADER = false;

VstResponse::VstResponse(ResponseCode code, uint64_t id)
    : GeneralResponse(code), _messageId(id) {
  _contentType = ContentType::VPACK;
}

void VstResponse::reset(ResponseCode code) {
  _responseCode = code;
  _headers.clear();
  _contentType = ContentType::VPACK;
  _generateBody = false;  // payload has to be set
}

void VstResponse::addPayload(VPackSlice const& slice, velocypack::Options const* options,
                             bool resolveExternals) {
  if (!options) {
    options = &VPackOptions::Options::Defaults;
  }

  // only copy buffer if it contains externals
  if (resolveExternals) {
    bool resolveExt = VelocyPackHelper::hasNonClientTypes(slice, true, true);
    if (resolveExt) {  // resolve
      VPackBuffer<uint8_t> tmpBuffer;
      tmpBuffer.reserve(slice.byteSize());  // reserve space already
      VPackBuilder builder(tmpBuffer, options);
      VelocyPackHelper::sanitizeNonClientTypes(slice, VPackSlice::noneSlice(),
                                               builder, options, true, true, true);
      _vpackPayloads.push_back(std::move(tmpBuffer));
      return;
    }
  }

  // just copy
  _vpackPayloads.emplace_back(slice.byteSize());
  _vpackPayloads.back().append(slice.startAs<char const>(), slice.byteSize());
}

void VstResponse::addPayload(VPackBuffer<uint8_t>&& buffer,
                             velocypack::Options const* options, bool resolveExternals) {
  if (!options) {
    options = &VPackOptions::Options::Defaults;
  }

  // only copy buffer if it contains externals
  if (resolveExternals) {
    VPackSlice input(buffer.data());
    bool resolveExt = VelocyPackHelper::hasNonClientTypes(input, true, true);
    if (resolveExt) {  // resolve
      VPackBuffer<uint8_t> tmpBuffer;
      tmpBuffer.reserve(buffer.length());  // reserve space already
      VPackBuilder builder(tmpBuffer, options);
      VelocyPackHelper::sanitizeNonClientTypes(input, VPackSlice::noneSlice(),
                                               builder, options, true, true, true);
      _vpackPayloads.push_back(std::move(tmpBuffer));
      return;  // done
    }
  }
  _vpackPayloads.push_back(std::move(buffer));
}

void VstResponse::addRawPayload(VPackStringRef payload) {
  if (_vpackPayloads.empty()) {
    _vpackPayloads.push_back(VPackBuffer<uint8_t>());
  }
  _vpackPayloads.back().append(payload.data(), payload.length());
}

#if 0
VPackMessageNoOwnBuffer VstResponse::prepareForNetwork() {
  // initialize builder with vpackbuffer. then we do not need to
  // steal the header and can avoid the shared pointer
  VPackBuilder builder;
  builder.openArray();
  builder.add(VPackValue(int(1)));  // 1 == version
  builder.add(VPackValue(int(2)));  // 2 == response
  builder.add(VPackValue(static_cast<int>(meta::underlyingValue(_responseCode))));  // 3 == request - return code

  std::string currentHeader;
  builder.openObject();  // 4 == meta
  for (auto& item : _headers) {
    currentHeader.assign(item.first);
    int capState = 1;
    for (auto& it : currentHeader) {
      if (capState == 1) {
        // upper case
        it = ::toupper(it);
        capState = 0;
      } else if (capState == 0) {
        // normal case
        it = ::tolower(it);
        if (it == '-') {
          capState = 1;
        } else if (it == ':') {
          capState = 2;
        }
      }
    }
    builder.add(currentHeader, VPackValue(item.second));
  }
  builder.close();

  builder.close();
  _header = builder.steal();
  if (_vpackPayloads.empty()) {
    if (_generateBody) {
      LOG_TOPIC("db6b3", INFO, Logger::REQUESTS)
          << "Response should generate body but no data available";
      _generateBody = false;  // no body available
    }
    return VPackMessageNoOwnBuffer(VPackSlice(_header->data()),
                                   VPackSlice::noneSlice(), _messageId, _generateBody);
  } else {
    std::vector<VPackSlice> slices;
    for (auto const& buffer : _vpackPayloads) {
      slices.emplace_back(buffer.data());
    }
    return VPackMessageNoOwnBuffer(VPackSlice(_header->data()),
                                   std::move(slices), _messageId, _generateBody);
  }
}
#endif
