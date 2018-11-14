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

#include "Collection.h"

#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::maskings;

ParseResult<Collection> Collection::parse(VPackSlice const& def) {
  if (!def.isObject()) {
    return ParseResult<Collection>{
        ParseResult<Collection>::PARSE_FAILED,
        "expecting an object for collection definition", Collection()};
  }

  std::string type = "";
  std::vector<AttributeMasking> attributes;

  for (auto const& entry : VPackObjectIterator(def, false)) {
    std::string key = entry.key.copyString();

    if (key == "type") {
      if (!entry.value.isString()) {
        return ParseResult<Collection>{ParseResult<Collection>::PARSE_FAILED,
                                       "expecting a string for collection type",
                                       Collection()};
      }

      type = entry.value.copyString();
    }
  }

  CollectionSelection selection = CollectionSelection::FULL;

  if (type != "full") {
    selection = CollectionSelection::FULL;
  } else if (type != "ignore") {
    selection = CollectionSelection::IGNORE;
  } else if (type != "structure") {
    selection = CollectionSelection::STRUCTURE;
  } else {
    return ParseResult<Collection>{
        ParseResult<Collection>::PARSE_FAILED,
        "expecting unknown collection type '" + type + "'", Collection()};
  }

  return ParseResult<Collection>{ParseResult<Collection>::VALID, "",
                                 Collection(selection, attributes)};
}
