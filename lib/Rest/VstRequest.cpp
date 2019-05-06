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

#include "VstRequest.h"
#include "VstMessage.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/StringRef.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "Meta/conversion.h"

#include <stdexcept>

using namespace arangodb;
using namespace arangodb::basics;

VstRequest::VstRequest(ConnectionInfo const& connectionInfo,
                       VstInputMessage&& message, uint64_t messageId)
    : GeneralRequest(connectionInfo),
      _messageId(messageId),
      _message(std::move(message)),
      _validatedPayload(false),
      _vpackBuilder(nullptr) {
  _protocol = "vst";
  _contentType = ContentType::VPACK;
  _contentTypeResponse = ContentType::VPACK;
  parseHeaderInformation();
}

VPackSlice VstRequest::payload(VPackOptions const* options) {
  TRI_ASSERT(options != nullptr);

  if (_contentType == ContentType::JSON) {
    if (!_vpackBuilder) {
      arangodb::velocypack::StringRef json = _message.payload();
      if (!json.empty()) {
        _vpackBuilder = VPackParser::fromJson(json.data(), json.length());
      }
    }
    if (_vpackBuilder) {
      return _vpackBuilder->slice();
    }
  } else if (_contentType == ContentType::VPACK) {
    arangodb::velocypack::StringRef vpack = _message.payload();
    if (!vpack.empty()) {
      if (!_validatedPayload) {
        VPackOptions validationOptions = *options;  // intentional copy
        validationOptions.validateUtf8Strings = true;
        validationOptions.checkAttributeUniqueness = true;
        validationOptions.disallowExternals = true;
        validationOptions.disallowCustom = true;
        VPackValidator validator(&validationOptions);
        // will throw on error
        _validatedPayload = validator.validate(vpack.data(), vpack.length());
      }
      return VPackSlice(reinterpret_cast<uint8_t const*>(vpack.data()));
    }
  }
  return VPackSlice::noneSlice();  // no body
}

void VstRequest::setHeader(VPackSlice keySlice, VPackSlice valSlice) {
  if (!keySlice.isString() || !valSlice.isString()) {
    return;
  }

  std::string value = valSlice.copyString();
  std::string key = StringUtils::tolower(keySlice.copyString());
  std::string val = StringUtils::tolower(value);
  static const char* mime = "application/json";
  if (key == StaticStrings::Accept && val.length() >= 16 &&
      memcmp(val.data(), mime, 16) == 0) {
    _contentTypeResponse = ContentType::JSON;
    return;  // don't insert this header!!
  } else if (key == StaticStrings::ContentTypeHeader && val.length() >= 16 &&
             memcmp(val.data(), mime, 16) == 0) {
    _contentType = ContentType::JSON;
    return;  // don't insert this header!!
  }

  // must lower-case the header key
  _headers.emplace(std::move(key), std::move(value));
}

void VstRequest::parseHeaderInformation() {
  using namespace std;
  auto vHeader = _message.header();
  if (!vHeader.isArray() || vHeader.length() != 7) {
    LOG_TOPIC("0007b", WARN, Logger::COMMUNICATION) << "invalid VST message header";
    throw std::runtime_error("invalid VST message header");
  }

  try {
    TRI_ASSERT(vHeader.isArray());
    auto version = vHeader.at(0).getInt();                      // version
    auto type = vHeader.at(1).getInt();                         // type
    _databaseName = vHeader.at(2).copyString();                 // database
    _type = meta::toEnum<RequestType>(vHeader.at(3).getInt());  // request type
    _requestPath = vHeader.at(4).copyString();  // request (path)
    VPackSlice params = vHeader.at(5);          // parameter
    VPackSlice meta = vHeader.at(6);            // meta

    if (version != 1) {
      LOG_TOPIC("e7fe5", WARN, Logger::COMMUNICATION)
          << "invalid version in vst message";
    }
    if (type != 1) {
      LOG_TOPIC("d8a18", WARN, Logger::COMMUNICATION) << "not a VST request";
      return;
    }

    for (auto const& it : VPackObjectIterator(params, true)) {
      if (it.value.isArray()) {
        vector<string> tmp;
        for (auto const& itInner : VPackArrayIterator(it.value)) {
          tmp.emplace_back(itInner.copyString());
        }
        _arrayValues.emplace(it.key.copyString(), move(tmp));
      } else {
        _values.emplace(it.key.copyString(), it.value.copyString());
      }
    }

    for (auto const& it : VPackObjectIterator(meta, true)) {
      setHeader(it.key, it.value);
    }

    // fullUrl should not be necessary for Vst
    _fullUrl = _requestPath + "?";
    for (auto const& param : _values) {
      _fullUrl.append(param.first + "=" +
                      basics::StringUtils::urlEncode(param.second) + "&");
    }

    for (auto const& param : _arrayValues) {
      for (auto const& value : param.second) {
        _fullUrl.append(param.first);
        _fullUrl.append("[]=");
        _fullUrl.append(basics::StringUtils::urlEncode(value));
        _fullUrl.push_back('&');
      }
    }
    _fullUrl.pop_back();  // remove last &

  } catch (std::exception const& e) {
    throw std::runtime_error(
        std::string("Error during Parsing of VstHeader: ") + e.what());
  }
}
