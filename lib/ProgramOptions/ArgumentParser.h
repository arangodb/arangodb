////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_PROGRAM_OPTIONS_ARGUMENT_PARSER_H
#define ARANGODB_PROGRAM_OPTIONS_ARGUMENT_PARSER_H 1

#include "Basics/Common.h"

#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {
namespace options {

class ArgumentParser {
 public:
  explicit ArgumentParser(ProgramOptions* options) : _options(options) {}

  // get the name of the section for which help was requested, and "*" if only
  // --help was specified
  std::string helpSection(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
      std::string const current(argv[i]);

      if (current == "--") {
        break;
      }

      if (current.size() >= 6 && current.substr(0, 6) == "--help") {
        if (current.size() <= 7) {
          // show all non-hidden options
          return "*";
        }
        // show help just for a specific section
        return current.substr(7);
      }
    }
    return "";
  }

  // parse options from argc/argv. returns true if all is well, false otherwise
  // errors that occur during parse are reported to _options
  bool parse(int argc, char* argv[]) {
    // set context for parsing (used in error messages)
    _options->setContext("command-line options");

    std::string lastOption;
    bool optionsDone = false;

    for (int i = 1; i < argc; ++i) {
      std::string option;
      std::string value;
      std::string const current(argv[i]);

      if (!lastOption.empty()) {
        option = lastOption;
      }

      if (!option.empty()) {
        value = current;
      } else {
        if (current == "--") {
          optionsDone = true;
          continue;
        }

        option = current;

        size_t dashes = 0;

        if (!optionsDone) {
          if (option.substr(0, 2) == "--") {
            dashes = 2;
          } else if (option.substr(0, 1) == "-") {
            dashes = 1;
          }
        }

        if (dashes == 0) {
          _options->addPositional(option);
          continue;
        }

        option = option.substr(dashes);

        size_t const pos = option.find('=');

        if (pos == std::string::npos) {
          // only option
          if (dashes == 1) {
            option = _options->translateShorthand(option);
          }
          if (!_options->require(option)) {
            return false;
          }

          if (!_options->requiresValue(option)) {
            // option does not require a parameter
            std::string nextValue;
            if (i + 1 < argc) {
              // we have a next option. check if its true or false
              std::string const next(argv[i + 1]);
              if (next == "true" || next == "false" || next == "on" ||
                  next == "off" || next == "1" || next == "0") {
                nextValue = next;
                ++i;  // skip next argument when continuing the parsing
              }
            }
            if (!_options->setValue(option, nextValue)) {
              return false;
            }
          } else {
            // option requires a parameter
            lastOption = option;
          }
          continue;
        } else {
          // option = value
          value = option.substr(pos + 1);
          option = option.substr(0, pos);
          if (dashes == 1) {
            option = _options->translateShorthand(option);
          }
        }
      }

      if (!_options->setValue(option, value)) {
        return false;
      }
      lastOption = "";
    }

    // we got some previous option, but no value was specified for it
    if (!lastOption.empty()) {
      return _options->fail("no value specified for option '--" + lastOption +
                            "'");
    }

    // all is well
    _options->endPass();
    return true;
  }

 private:
  ProgramOptions* _options;
};
}  // namespace options
}  // namespace arangodb

#endif
