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

#ifndef ARANGODB_MASKINGS_PATH_H
#define ARANGODB_MASKINGS_PATH_H 1

#include "Basics/Common.h"

#include "Maskings/ParseResult.h"

namespace arangodb {
namespace maskings {
class Path {
 public:
  static ParseResult<Path> parse(std::string const&);

 public:
  Path() : _wildcard(false) {}

  Path(bool wildcard, bool any, std::vector<std::string> const& components)
      : _wildcard(wildcard), _any(any), _components(components) {}

  bool match(std::vector<std::string> const& path) const;

 private:
  bool _wildcard;
  bool _any;
  std::vector<std::string> _components;
};
}  // namespace maskings
}  // namespace arangodb

#endif
