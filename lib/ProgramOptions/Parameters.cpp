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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ProgramOptions/Parameters.h"

#include <regex>

namespace arangodb::options {

std::string removeCommentsFromNumber(std::string const& value) {
  // note: these regex objects are built upon every invocation of this function.
  // this is necessary because this function is called during static
  // initialization, and we need to avoid an init order fiasco with TU-local
  // regexes and other TUs.

  // replace trailing comments
  auto noComment =
      std::regex_replace(value, std::regex("#.*$", std::regex::ECMAScript), "");

  // replace leading spaces, replace trailing spaces
  return std::regex_replace(
      noComment, std::regex("^[ \t]+|[ \t]+$", std::regex::ECMAScript), "");
}

}  // namespace arangodb::options
