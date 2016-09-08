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

#include "VppRequest.h"
#include "VppMessage.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/StringUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "Meta/conversion.h"

#include <stdexcept>

using namespace arangodb;
using namespace arangodb::basics;

namespace {
std::string const& lookupStringInMap(
    std::unordered_map<std::string, std::string> const& map,
    std::string const& key, bool& found) {
  auto it = map.find(key);
  if (it != map.end()) {
    found = true;
    return it->second;
  }

  found = false;
  return StaticStrings::Empty;
}
}

VppRequest::VppRequest(ConnectionInfo const& connectionInfo,
                       VppInputMessage&& message, uint64_t messageId)
    : GeneralRequest(connectionInfo),
      _message(std::move(message)),
      _headers(nullptr),
      _messageId(messageId) {
  _protocol = "vpp";
  _contentType = ContentType::VPACK;
  _contentTypeResponse = ContentType::VPACK;
  parseHeaderInformation();
  _user = "root";
}

VPackSlice VppRequest::payload(VPackOptions const* options) {
  return _message.payload();
}

std::unordered_map<std::string, std::string> const& VppRequest::headers()
    const {
  if (!_headers) {
    using namespace std;
    _headers = make_unique<unordered_map<string, string>>();
    VPackSlice meta = _message.header().at(6);  // get meta
    for (auto const& it : VPackObjectIterator(meta)) {
      // must lower-case the header key
      _headers->emplace(StringUtils::tolower(it.key.copyString()),
                        it.value.copyString());
    }
  }
  return *_headers;
}

std::string const& VppRequest::header(std::string const& key,
                                      bool& found) const {
  headers();
  return lookupStringInMap(*_headers, key, found);
}

std::string const& VppRequest::header(std::string const& key) const {
  bool unused = true;
  return header(key, unused);
}

void VppRequest::parseHeaderInformation() {
  using namespace std;
  auto vHeader = _message.header();
  try {
    TRI_ASSERT(vHeader.isArray());
    // vHeader.at(0).getInt()  //version
    // vHeader.at(1).getInt()  //type
    _databaseName = vHeader.at(2).copyString();                 // database
    _type = meta::toEnum<RequestType>(vHeader.at(3).getInt());  // request type
    _requestPath = vHeader.at(4).copyString();  // request (path)
    VPackSlice params = vHeader.at(5);          // parameter

    for (auto const& it : VPackObjectIterator(params)) {
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

    // fullUrl should not be necessary for Vpp
    _fullUrl = _requestPath + "?";
    for (auto const& param : _values) {
      _fullUrl.append(param.first + "=" +
                      basics::StringUtils::urlEncode(param.second) + "&");
    }

    for (auto const& param : _arrayValues) {
      for (auto const& value : param.second) {
        _fullUrl.append(param.first + "[]=" +
                        basics::StringUtils::urlEncode(value) + "&");
      }
    }
    _fullUrl.pop_back();

  } catch (std::exception const& e) {
    throw std::runtime_error(
        std::string("Error during Parsing of VppHeader: ") + e.what());
  }
}

std::string const& VppRequest::value(std::string const& key,
                                     bool& found) const {
  return lookupStringInMap(_values, key, found);
}

std::string const& VppRequest::value(std::string const& key) const {
  bool unused = true;
  return value(key, unused);
}
