////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Maskings/RandomStringMask.h"
#include "Maskings/RandomMask.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::maskings;

void arangodb::maskings::InstallMaskings() {
  AttributeMasking::installMasking("randomString", RandomStringMask::create);
  AttributeMasking::installMasking("random", RandomMask::create);
}

std::unordered_map<std::string, ParseResult<AttributeMasking> (*)(
                                    Path, Maskings*, velocypack::Slice)>
    AttributeMasking::_maskings;

ParseResult<AttributeMasking> AttributeMasking::parse(Maskings* maskings,
                                                      velocypack::Slice def) {
  if (!def.isObject()) {
    return ParseResult<AttributeMasking>(
        ParseResult<AttributeMasking>::PARSE_FAILED,
        "expecting an object for collection definition");
  }

  std::string_view path;
  std::string type;

  for (auto entry : VPackObjectIterator(def, false)) {
    auto key = entry.key.stringView();

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

      path = entry.value.stringView();
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

  auto const& it = _maskings.find(type);

  if (it == _maskings.end()) {
    return ParseResult<AttributeMasking>(
        ParseResult<AttributeMasking>::UNKNOWN_TYPE,
        absl::StrCat("unknown attribute masking type '", type, "'"));
  }

  return it->second(ap.result, maskings, def);
}

bool AttributeMasking::match(std::vector<std::string_view> const& path) const {
  return _path.match(path);
}
