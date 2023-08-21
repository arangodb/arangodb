////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/Common.h"

#include "Maskings/ParseResult.h"

#include <string>
#include <string_view>
#include <vector>

namespace arangodb::maskings {
class Path {
 public:
  static ParseResult<Path> parse(std::string_view def);

 public:
  Path() : _wildcard(false), _any(false) {}

  Path(bool wildcard, bool any, std::vector<std::string> const& components)
      : _wildcard(wildcard), _any(any), _components(components) {}

  bool match(std::vector<std::string_view> const& path) const;

 private:
  bool _wildcard;
  bool _any;
  std::vector<std::string> _components;
};
}  // namespace arangodb::maskings
