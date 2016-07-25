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

VppRequest::VppRequest(ConnectionInfo const& connectionInfo,
                       VPackBuffer<uint8_t>&& header, size_t length)
    : GeneralRequest(connectionInfo),
      _contentLength(0),
      _header(std::move(header)),
      _cookies(std::unordered_map<std::string, std::string>() /*TODO remove*/) {
  if (0 < length) {
    _contentType = ContentType::VPACK;
    _contentTypeResponse = ContentType::VPACK;
    parseHeader();
  }
}

VPackSlice VppRequest::payload(VPackOptions const* options) {
  VPackValidator validator;
  validator.validate(_payload.data(), _payload.size());
  return VPackSlice(_payload.data());
}

void VppRequest::parseHeader() {}
