////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
#include "Maskings/XifyFront.h"

using namespace arangodb;
using namespace arangodb::maskings;

ParseResult<AttributeMasking> AttributeMasking::parse(Maskings* maskings,
                                                      VPackSlice const& def) {
  if (!def.isObject()) {
    return ParseResult<AttributeMasking>(
        ParseResult<AttributeMasking>::PARSE_FAILED,
        "expecting an object for collection definition");
  }

  std::string path = "";
  std::string type = "";
  uint64_t length = 2;
  uint64_t seed = 0;
  bool hash = false;

  for (auto const& entry : VPackObjectIterator(def, false)) {
    std::string key = entry.key.copyString();

    if (key == "type") {
      if (!entry.value.isString()) {
        return ParseResult<AttributeMasking>(
            ParseResult<AttributeMasking>::ILLEGAL_PARAMETER,
            "type must be a string");
      }

      type = entry.value.copyString();
    } else if (key == "path") {
      if (!entry.value.isString()) {
        return ParseResult<AttributeMasking>(
            ParseResult<AttributeMasking>::ILLEGAL_PARAMETER,
            "path must be a string");
      }

      path = entry.value.copyString();
    } else if (key == "unmaskedLength") {
      if (!entry.value.isInteger()) {
        return ParseResult<AttributeMasking>(
            ParseResult<AttributeMasking>::ILLEGAL_PARAMETER,
            "length must be an integer");
      }

      length = entry.value.getInt();
    } else if (key == "hash") {
      if (!entry.value.isBool()) {
        return ParseResult<AttributeMasking>(
            ParseResult<AttributeMasking>::ILLEGAL_PARAMETER,
            "hash must be an integer");
      }

      hash = entry.value.getBool();
    } else if (key == "seed") {
      if (!entry.value.isInteger()) {
        return ParseResult<AttributeMasking>(
            ParseResult<AttributeMasking>::ILLEGAL_PARAMETER,
            "seed must be an integer");
      }

      seed = entry.value.getInt();
    }
  }

  if (path.empty()) {
    return ParseResult<AttributeMasking>(
        ParseResult<AttributeMasking>::ILLEGAL_PARAMETER,
        "path must not be empty");
  }

  ParseResult<Path> ap = Path::parse(path);

  if (ap.status != ParseResult<Path>::VALID) {
    return ParseResult<AttributeMasking>(
        (ParseResult<AttributeMasking>::StatusCode)(int)ap.status, ap.message);
  }

  if (type == "xify_front") {
    if (length < 1) {
      return ParseResult<AttributeMasking>(
          ParseResult<AttributeMasking>::ILLEGAL_PARAMETER,
          "expecting length to be at least for xify_front");
    }

    return ParseResult<AttributeMasking>(AttributeMasking(
        ap.result, new XifyFront(maskings, length, hash, seed)));
  }

  return ParseResult<AttributeMasking>(
      ParseResult<AttributeMasking>::UNKNOWN_TYPE,
      "expecting unknown attribute masking type '" + type + "'");
}

bool AttributeMasking::match(std::vector<std::string> const& path) const {
  return _path.match(path);
}
