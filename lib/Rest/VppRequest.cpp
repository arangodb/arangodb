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

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/conversions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

std::string const VppRequest::_bla = "";
std::unordered_map<std::string, std::string> VppRequest::_blam =
    std::unordered_map<std::string, std::string>();

VppRequest::VppRequest(ConnectionInfo const& connectionInfo,
                       VPackMessage&& message)
    : GeneralRequest(connectionInfo),
      _message(std::move(message)),
      _headers(nullptr) {
  if (message._payload != VPackSlice::noneSlice()) {
    _contentType = ContentType::VPACK;
    _contentTypeResponse = ContentType::VPACK;
    _protocol = "vpp";
  }
}

VPackSlice VppRequest::payload(VPackOptions const* options) {
  // TODO - handle options??
  return _message._payload;
}

std::unordered_map<std::string, std::string> const& VppRequest::headers()
    const {
  if (!_headers) {
    using namespace std;
    _headers = make_unique<unordered_map<string, string>>();
    TRI_ASSERT(_message._header.isObject());
    for (auto const& it : VPackObjectIterator(_message._header)) {
      _headers->emplace(it.key.copyString(), it.value.copyString());
    }
  }
  return *_headers;
}

std::string const& VppRequest::header(std::string const& key,
                                      bool& found) const {
  // we need to use headers as we MUST return a ref to string
  headers();
  auto it = _headers->find(key);
  if (it != _headers->end()) {
    found = true;
    return it->second;
  }
  found = false;
  return StaticStrings::Empty;
}

std::string const& VppRequest::header(std::string const& key) const {
  bool unused = true;
  return header(key, unused);
}
