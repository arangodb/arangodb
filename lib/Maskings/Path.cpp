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

#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::maskings;

ParseResult<Path> Path::parse(std::string const& def) {
  if (def.empty()) {
    return ParseResult<Path>(ParseResult<Path>::ILLEGAL_PARAMETER,
                             "path must not be empty");
  }

  std::vector<std::string> components;

  if (def == "*") {
    return ParseResult<Path>(Path(false, true, components));
  }

  bool wildcard = false;

  if (def[0] == '.') {
    wildcard = true;
  }

  uint8_t const* p = reinterpret_cast<uint8_t const*>(def.c_str());
  int32_t off = 0;
  int32_t len = static_cast<int32_t>(def.size());
  UChar32 ch;

  if (wildcard) {
    U8_NEXT(p, off, len, ch);
  }

  std::string buffer;

  while (off < len) {
    U8_NEXT(p, off, len, ch);

    if (ch < 0) {
      return ParseResult<Path>(ParseResult<Path>::ILLEGAL_PARAMETER,
                               "path '" + def + "' contains illegal UTF-8");
    } else if (ch == 46) {
      if (buffer.size() == 0) {
        return ParseResult<Path>(ParseResult<Path>::ILLEGAL_PARAMETER,
                                 "path '" + def +
                                     "' contains an empty component");
      }

      components.push_back(buffer);
      buffer.clear();
    } else if (ch == 96 || ch == 180) {  // windows does not like U'`' and U'´'
      UChar32 quote = ch;
      U8_NEXT(p, off, len, ch);

      if (ch < 0) {
        return ParseResult<Path>(ParseResult<Path>::ILLEGAL_PARAMETER,
                                 "path '" + def + "' contains illegal UTF-8");
      }

      while (off < len && ch != quote) {
        basics::Utf8Helper::appendUtf8Character(buffer, ch);
        U8_NEXT(p, off, len, ch);

        if (ch < 0) {
          return ParseResult<Path>(ParseResult<Path>::ILLEGAL_PARAMETER,
                                   "path '" + def + "' contains illegal UTF-8");
        }
      }

      if (ch != quote) {
        return ParseResult<Path>(ParseResult<Path>::ILLEGAL_PARAMETER,
                                 "path '" + def +
                                     "' contains an unbalanced quote");
      }

      U8_NEXT(p, off, len, ch);

      if (ch < 0) {
        return ParseResult<Path>(ParseResult<Path>::ILLEGAL_PARAMETER,
                                 "path '" + def + "' contains illegal UTF-8");
      }
    } else {
      basics::Utf8Helper::appendUtf8Character(buffer, ch);
    }
  }

  if (buffer.size() == 0) {
    return ParseResult<Path>(ParseResult<Path>::ILLEGAL_PARAMETER,
                             "path '" + def + "' contains an empty component");
  }

  components.push_back(buffer);

  if (components.empty()) {
    return ParseResult<Path>(ParseResult<Path>::ILLEGAL_PARAMETER,
                             "path '" + def + "' contains no component");
  }

  return ParseResult<Path>(Path(wildcard, false, components));
}

bool Path::match(std::vector<std::string> const& path) const {
  size_t cs = _components.size();
  size_t ps = path.size();

  if (_any) {
    return true;
  }

  if (!_wildcard) {
    if (ps != cs) {
      return false;
    }
  }

  if (ps < cs) {
    return false;
  }

  size_t pi = ps;
  size_t ci = cs;

  while (0 < ci) {
    if (path[pi - 1] != _components[ci - 1]) {
      return false;
    }

    --pi;
    --ci;
  }

  return true;
}
