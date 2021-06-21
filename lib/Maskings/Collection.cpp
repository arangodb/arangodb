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

#include "Collection.h"

#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::maskings;

ParseResult<Collection> Collection::parse(Maskings* maskings, VPackSlice const& def) {
  if (!def.isObject()) {
    return ParseResult<Collection>(
        ParseResult<Collection>::PARSE_FAILED,
        "expecting an object for collection definition");
  }

  std::string type = "";
  std::vector<AttributeMasking> attributes;

  for (auto const& entry : VPackObjectIterator(def, false)) {
    std::string key = entry.key.copyString();

    if (key == "type") {
      if (!entry.value.isString()) {
        return ParseResult<Collection>(
            ParseResult<Collection>::ILLEGAL_PARAMETER,
            "expecting a string for collection type");
      }

      type = entry.value.copyString();
    } else if (key == "maskings") {
      if (!entry.value.isArray()) {
        return ParseResult<Collection>(
            ParseResult<Collection>::ILLEGAL_PARAMETER,
            "expecting an array for collection maskings");
      }

      for (auto const& mask : VPackArrayIterator(entry.value)) {
        ParseResult<AttributeMasking> am = AttributeMasking::parse(maskings, mask);

        if (am.status != ParseResult<AttributeMasking>::VALID) {
          return ParseResult<Collection>((ParseResult<Collection>::StatusCode)(
                                             int)am.status,
                                         am.message);
        }

        attributes.push_back(am.result);
      }
    }
  }

  CollectionSelection selection = CollectionSelection::FULL;

  if (type == "full") {
    selection = CollectionSelection::FULL;
  } else if (type == "exclude") {
    selection = CollectionSelection::EXCLUDE;
  } else if (type == "masked") {
    selection = CollectionSelection::MASKED;
  } else if (type == "structure") {
    selection = CollectionSelection::STRUCTURE;
  } else {
    return ParseResult<Collection>(ParseResult<Collection>::UNKNOWN_TYPE,
                                   "found unknown collection type '" + type +
                                       "'");
  }

  return ParseResult<Collection>(Collection(selection, attributes));
}

MaskingFunction* Collection::masking(std::vector<std::string> const& path) {
  for (auto const& m : _maskings) {
    if (m.match(path)) {
      return m.func();
    }
  }

  return nullptr;
}
