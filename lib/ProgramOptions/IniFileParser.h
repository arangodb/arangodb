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

#ifndef ARANGODB_PROGRAM_OPTIONS_INI_FILE_PARSER_H
#define ARANGODB_PROGRAM_OPTIONS_INI_FILE_PARSER_H 1

#include <regex>
#include <set>
#include <string>

namespace arangodb {
namespace options {
class ProgramOptions;

class IniFileParser {
 public:
  explicit IniFileParser(ProgramOptions* options);

  // parse a config file. returns true if all is well, false otherwise
  // errors that occur during parse are reported to _options
  bool parse(std::string const& filename, bool endPassAfterwards);

  // parse a config file, with the contents already read into <buf>.
  // returns true if all is well, false otherwise
  // errors that occur during parse are reported to _options
  bool parseContent(std::string const& filename, std::string const& buf, bool endPassAfterwards);

 private:
  ProgramOptions* _options;
  std::set<std::string> _seen;

  struct {
    std::regex comment;
    std::regex section;
    std::regex enterpriseSection;
    std::regex communitySection;
    std::regex assignment;
    std::regex include;
  } _matchers;
};
}  // namespace options
}  // namespace arangodb

#endif
