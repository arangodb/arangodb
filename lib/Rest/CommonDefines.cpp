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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <ostream>
#include <string>

#include "CommonDefines.h"

#include "Basics/StaticStrings.h"

namespace arangodb {
namespace rest {

std::string contentTypeToString(ContentType type) {
  switch (type) {
    case ContentType::VPACK:
      return StaticStrings::MimeTypeVPack;
    case ContentType::TEXT:
      return StaticStrings::MimeTypeText;
    case ContentType::HTML:
      return StaticStrings::MimeTypeHtml;
    case ContentType::DUMP:
      return StaticStrings::MimeTypeDump;
    case ContentType::CUSTOM:
      return StaticStrings::Empty;  // use value from headers
    case ContentType::UNSET:
    case ContentType::JSON:
    default:
      return StaticStrings::MimeTypeJson;
  }
}

ContentType stringToContentType(std::string const& val, ContentType def) {
  if (val.size() >= StaticStrings::MimeTypeJsonNoEncoding.size() &&
      val.compare(0, StaticStrings::MimeTypeJsonNoEncoding.size(),
                  StaticStrings::MimeTypeJsonNoEncoding) == 0) {
    return ContentType::JSON;
  }
  if (val == StaticStrings::MimeTypeVPack) {
    return ContentType::VPACK;
  }
  if (val.size() >= 25 && val.compare(0, 25, "application/x-arango-dump") == 0) {
    return ContentType::DUMP;
  }
  if (val.size() >= 10 && val.compare(0, 10, "text/plain") == 0) {
    return ContentType::TEXT;
  }
  if (val.size() >= 9 && val.compare(0, 9, "text/html") == 0) {
    return ContentType::HTML;
  }
  return def;
}

}  // namespace rest
}  // namespace arangodb
