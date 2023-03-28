////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
  // note:
  // this function is already called during static initialization.
  // the following regex objects are function-local statics, because
  // we cannot have them statically initialized on the TU level.
  static std::regex const removeComments("#.*$", std::regex::ECMAScript);
  static std::regex const removeTabs("^[ \t]+|[ \t]+$", std::regex::ECMAScript);

  // replace trailing comments
  auto noComment = std::regex_replace(value, removeComments, "");
  // replace leading spaces, replace trailing spaces
  return std::regex_replace(noComment, removeTabs, "");
}

}  // namespace arangodb::options
