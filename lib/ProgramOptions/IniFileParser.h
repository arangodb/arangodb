////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"

#include <fstream>
#include <iostream>
#include <regex>

#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {
namespace options {

class IniFileParser {
 public:
  explicit IniFileParser(ProgramOptions* options) : _options(options) {
    // a line with just comments, e.g. #... or ;...
    _matchers.comment = std::regex("^[ \t]*([#;].*)?$",
                                   std::regex::nosubs | std::regex::ECMAScript);
    // a line that starts a section, e.g. [server]
    _matchers.section = std::regex("^[ \t]*\\[([-_A-Za-z0-9]*)\\][ \t]*$",
                                   std::regex::ECMAScript);
    // a line that assigns a value to a named variable
    _matchers.assignment = std::regex(
        "^[ \t]*(([-_A-Za-z0-9]*\\.)?[-_A-Za-z0-9]*)[ \t]*=[ \t]*(.*?)?[ \t]*$",
        std::regex::ECMAScript);
    // an include line
    _matchers.include = std::regex(
        "^[ \t]*@include[ \t]*([-_A-Za-z0-9]*)[ \t]*$",
        std::regex::ECMAScript);
  }

  // parse a config file. returns true if all is well, false otherwise
  // errors that occur during parse are reported to _options
  bool parse(std::string const& filename) {
    std::ifstream ifs(filename, std::ifstream::in);

    if (!ifs.is_open()) {
      return _options->fail("unable to open configuration file '" + filename + "'");
    }

    std::string currentSection;
    size_t lineNumber = 0;

    while (ifs.good()) {
      std::string line;
      ++lineNumber;

      std::getline(ifs, line);

      if (std::regex_match(line, _matchers.comment)) {
        // skip over comments
        continue;
      }

      // set context for parsing (used in error messages)
      _options->setContext("config file '" + filename + "', line #" +
                           std::to_string(lineNumber));

      std::smatch match;
      if (std::regex_match(line, match, _matchers.section)) {
        // found section
        currentSection = match[1].str();
      } else if (std::regex_match(line, match, _matchers.include)) {
        // found include
        std::string option;
        std::string value(match[1].str());
        
        _includes.emplace_back(value);
      } else if (std::regex_match(line, match, _matchers.assignment)) {
        // found assignment
        std::string option;
        std::string value(match[3].str());

        if (currentSection.empty() || !match[2].str().empty()) {
          // use option as specified
          option = match[1].str();
        } else {
          // use option prefixed with current section
          option = currentSection + "." + match[1].str();
        }

        if (!_options->setValue(option, value)) {
          return false;
        }
      } else {
        // unknown type of line. cannot handle it
        return _options->fail("unknown line type");
      }
    }

    // all is well
    _options->endPass();
    return true;
  }

  // seen includes
  std::vector<std::string> const& includes() const {
    return _includes;
  }

 private:
  ProgramOptions* _options;
  std::vector<std::string> _includes;

  struct {
    std::regex comment;
    std::regex section;
    std::regex assignment;
    std::regex include;
  } _matchers;
};
}
}

#endif
