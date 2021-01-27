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
#include "Basics/VelocyPackDumper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/tri-strings.h"
#include "Meta/conversion.h"
#include "Rest/VstRequest.h"

using namespace arangodb;
using namespace arangodb::basics;

VstResponse::VstResponse(ResponseCode code, uint64_t id)
    : GeneralResponse(code, id) {
  _contentType = ContentType::VPACK;
}

void VstResponse::reset(ResponseCode code) {
  _responseCode = code;
  _headers.clear();
  _contentType = ContentType::VPACK;
  _payload.clear();
}

void VstResponse::addPayload(VPackSlice const& slice,
                             VPackOptions const* options,
                             bool resolveExternals) {
  if (_contentType == rest::ContentType::VPACK &&
      _contentTypeRequested == rest::ContentType::JSON) {
    // content type was set by a handler to VPACK but the client requested JSON
    // as we have a slice at hand we are able to reply with JSON easily
    _contentType = rest::ContentType::JSON;
  }
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
      if (_contentType == rest::ContentType::VPACK) {
        if (_payload.empty()) {
          _payload = std::move(tmpBuffer);
        } else {
          _payload.append(tmpBuffer.data(), tmpBuffer.size());
        }
      } else if (_contentType == rest::ContentType::JSON) {
        VPackSlice finalSlice(tmpBuffer.data());
        StringBuffer plainBuffer;
        arangodb::basics::VelocyPackDumper dumper(&plainBuffer, options);
        dumper.dumpValue(finalSlice);
        _payload.reset();
        _payload.append(plainBuffer.data(), plainBuffer.length());
      } else {
        _payload.reset();
        _payload.append(slice.start(), slice.byteSize());
      }
      return;
    }
  }

  if (_contentType == rest::ContentType::VPACK) {
    // just copy
    _payload.append(slice.startAs<char>(), slice.byteSize());
  } else if (_contentType == rest::ContentType::JSON) {
    // simon: usually we escape unicode char sequences,
    // but JSON over VST is not consumed by node.js or browsers
    velocypack::ByteBufferSinkImpl<uint8_t> sink(&_payload);
    VPackDumper dumper(&sink, options);
    dumper.dump(slice);
  } else {
    _payload.reset();
    _payload.append(slice.start(), slice.byteSize());
  }
}

void VstResponse::addPayload(VPackBuffer<uint8_t>&& buffer,
                             velocypack::Options const* options, bool resolveExternals) {
  if (_contentType == rest::ContentType::VPACK &&
      _contentTypeRequested == rest::ContentType::JSON) {
    // content type was set by a handler to VPACK but the client wants JSON
    // as we have a slice at had we are able to reply with JSON
    _contentType = rest::ContentType::JSON;
  }
  if (!options) {
    options = &VPackOptions::Options::Defaults;
  }
  
  auto handleBuffer = [this, options](VPackBuffer<uint8_t>&& buff) {
    if (ADB_UNLIKELY(_contentType == rest::ContentType::JSON)) {
      // simon: usually we escape unicode char sequences,
      // but JSON over VST is not consumed by node.js or browsers
      velocypack::ByteBufferSinkImpl<uint8_t> sink(&_payload);
      VPackDumper dumper(&sink, options);
      dumper.dump(VPackSlice(buff.data()));
    } else {
      if (_payload.empty()) {
        _payload = std::move(buff);
      } else {
        _payload.append(buff.data(), buff.size());
      }
    }
  };

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
      handleBuffer(std::move(tmpBuffer));
      return;
    }
    // fallthrough to non-resolve case
  }
  handleBuffer(std::move(buffer));
}

void VstResponse::addRawPayload(VPackStringRef payload) {
  _payload.append(payload.data(), payload.length());
}

void VstResponse::writeMessageHeader(VPackBuffer<uint8_t>& buffer) const {
  VPackBuilder builder(buffer);
  VPackArrayBuilder array(&builder, /*unindexed*/true);
  builder.add(VPackValue(int(1)));  // 1 == version
  builder.add(VPackValue(int(2)));  // 2 == response
  builder.add(VPackValue(static_cast<int>(meta::underlyingValue(_responseCode))));  // 3 == request - return code
  
  auto fixCase = [](std::string& tmp) {
    int capState = 1;
    for (auto& it : tmp) {
      if (capState == 1) {
        // upper case
        it = StringUtils::toupper(it);
        capState = 0;
      } else if (capState == 0) {
        // normal case
        it = StringUtils::tolower(it);
        if (it == '-') {
          capState = 1;
        } else if (it == ':') {
          capState = 2;
        }
      }
    }
  };
  
  std::string currentHeader;
  VPackObjectBuilder meta(&builder, /*unindexed*/true);  // 4 == meta
  for (auto& item : _headers) {

    if (_contentType != ContentType::CUSTOM &&
        item.first.compare(0, StaticStrings::ContentTypeHeader.size(),
                           StaticStrings::ContentTypeHeader) == 0) {
      continue;
    }
    currentHeader.assign(item.first);
    fixCase(currentHeader);
    builder.add(currentHeader, VPackValue(item.second));
  }
  if (!generateBody()) {
    std::string len = std::to_string(_payload.size());
    builder.add(StaticStrings::ContentLength, VPackValue(len));
  }
  if (_contentType != ContentType::VPACK &&
      _contentType != ContentType::CUSTOM) { // fuerte uses VPack as default
    currentHeader = StaticStrings::ContentTypeHeader;
    fixCase(currentHeader);
    builder.add(currentHeader, VPackValue(rest::contentTypeToString(_contentType)));
  }
}
