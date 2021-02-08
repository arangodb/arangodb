////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "AttributeMasking.h"

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "Maskings/RandomStringMask.h"
#include "Maskings/RandomMask.h"

using namespace arangodb;
using namespace arangodb::maskings;

void arangodb::maskings::InstallMaskings() {
  AttributeMasking::installMasking("randomString", RandomStringMask::create);
  AttributeMasking::installMasking("random", RandomMask::create);
}

std::unordered_map<std::string, ParseResult<AttributeMasking> (*)(Path, Maskings*, VPackSlice const&)> AttributeMasking::_maskings;

ParseResult<AttributeMasking> AttributeMasking::parse(Maskings* maskings,
                                                      VPackSlice const& def) {
  if (!def.isObject()) {
    return ParseResult<AttributeMasking>(
        ParseResult<AttributeMasking>::PARSE_FAILED,
        "expecting an object for collection definition");
  }

  std::string path = "";
  std::string type = "";

  for (auto const& entry : VPackObjectIterator(def, false)) {
    std::string key = entry.key.copyString();

    if (key == "type") {
      if (!entry.value.isString()) {
        return ParseResult<AttributeMasking>(ParseResult<AttributeMasking>::ILLEGAL_PARAMETER,
                                             "type must be a string");
      }

      type = entry.value.copyString();
    } else if (key == "path") {
      if (!entry.value.isString()) {
        return ParseResult<AttributeMasking>(ParseResult<AttributeMasking>::ILLEGAL_PARAMETER,
                                             "path must be a string");
      }

      path = entry.value.copyString();
    }
  }

  if (path.empty()) {
    return ParseResult<AttributeMasking>(ParseResult<AttributeMasking>::ILLEGAL_PARAMETER,
                                         "path must not be empty");
  }

  ParseResult<Path> ap = Path::parse(path);

  if (ap.status != ParseResult<Path>::VALID) {
    return ParseResult<AttributeMasking>(
        (ParseResult<AttributeMasking>::StatusCode)(int)ap.status, ap.message);
  }

  auto const& it = _maskings.find(type);

  if (it == _maskings.end()) {
    return ParseResult<AttributeMasking>(
        ParseResult<AttributeMasking>::UNKNOWN_TYPE,
        "unknown attribute masking type '" + type + "'");
  }

  return it->second(ap.result, maskings, def);
}

bool AttributeMasking::match(std::vector<std::string> const& path) const {
  return _path.match(path);
}
