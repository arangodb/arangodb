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
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::maskings;

ParseResult<Path> Path::parse(std::string const& def) {
  if (def.empty()) {
    return ParseResult<Path>(ParseResult<Path>::ILLEGAL_PARAMETER,
                             "path must not be empty");
  }

  bool wildcard = false;

  if (def[0] == '.') {
    wildcard = true;
  }

  char const* p = def.c_str();
  char const* e = p + def.size();

  if (wildcard) {
    ++p;
  }

  std::vector<std::string> components;
  std::string buffer;

  while (p < e) {
    if (*p == '.') {
      if (buffer.size() == 0) {
        return ParseResult<Path>(
            ParseResult<Path>::ILLEGAL_PARAMETER,
            "path '" + def + "' contains an empty component");
      }

      ++p;
      components.push_back(buffer);
      buffer.clear();
    } else if (*p == 96) {  // backtick `
      ++p;

      while (p < e && *p != 96) {
        buffer.push_back(*p++);
      }

      if (p == e) {
        return ParseResult<Path>(
            ParseResult<Path>::ILLEGAL_PARAMETER,
            "path '" + def + "' contains an unbalanced quote");
      }

      ++p;
    } else if (p[0] == -62 &&
               p[1] == -76) {  // there is also a 0 at *e, so p[1] is ok
      p += 2;

      while (p < e - 1 && (p[0] != -62 || p[1] != -76)) {
        buffer.push_back(*p++);
      }

      if (p == e) {
        return ParseResult<Path>(
            ParseResult<Path>::ILLEGAL_PARAMETER,
            "path '" + def + "' contains an unbalanced quote");
      }

      p += 2;
    } else if (p[0] == -76 &&
               p[1] == -62) {  // there is also a 0 at *e, so p[1] is ok
      p += 2;

      while (p < e - 1 && (p[0] != -76 || p[1] != -62)) {
        buffer.push_back(*p++);
      }

      if (p == e) {
        return ParseResult<Path>(
            ParseResult<Path>::ILLEGAL_PARAMETER,
            "path '" + def + "' contains an unbalanced quote");
      }

      p += 2;
    } else {
      buffer.push_back(*p++);
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

  return ParseResult<Path>(Path(wildcard, components));
}

bool Path::match(std::vector<std::string> const& path) const {
  size_t cs = _components.size();
  size_t ps = path.size();

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
