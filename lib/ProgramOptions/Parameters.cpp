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

namespace {
std::regex const removeComments("#.*$", std::regex::ECMAScript);
std::regex const removeTabs("^[ \t]+|[ \t]+$", std::regex::ECMAScript);
std::regex const contextPrefix("([a-zA-Z0-9]*)=(.*)", std::regex::ECMAScript);
}  // namespace

namespace arangodb {
namespace options {

std::string removeCommentsFromNumber(std::string const& value) {
  // replace trailing comments
  auto noComment = std::regex_replace(value, ::removeComments, "");
  // replace leading spaces, replace trailing spaces
  return std::regex_replace(noComment, ::removeTabs, "");
}

void parseContext(std::string const& rawValue, std::string& context,
                  std::string& value) {
  std::smatch m;
  bool match = std::regex_match(rawValue, m, contextPrefix);

  if (match) {
    context = m[1].str();
    value = m[2].str();
  } else {
    context = "";
    value = rawValue;
  }
}

}  // namespace options
}  // namespace arangodb
