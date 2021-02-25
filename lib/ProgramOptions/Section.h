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

#ifndef ARANGODB_PROGRAM_OPTIONS_SECTION_H
#define ARANGODB_PROGRAM_OPTIONS_SECTION_H 1

#include "Basics/Common.h"
#include "ProgramOptions/Option.h"

#include <map>

namespace arangodb {
namespace options {

// a single program options section
struct Section {
  // sections are default copy-constructible and default movable

  Section(std::string const& name, std::string const& description,
          std::string const& link, std::string const& alias, 
          bool hidden, bool obsolete)
      : name(name),
        description(description),
        link(link),
        alias(alias),
        hidden(hidden),
        obsolete(obsolete),
        enterpriseOnly(false) {}

  // get display name for the section
  std::string displayName() const { return alias.empty() ? name : alias; }
  
  // whether or not the section has (displayable) options
  bool hasOptions() const;

  // print help for a section
  // the special search string "." will show help for all sections, even if
  // hidden
  void printHelp(std::string const& search, size_t tw, size_t ow, bool colors) const;

  // determine display width for a section
  size_t optionsWidth() const;

  std::string name;
  std::string description;
  std::string link;
  std::string alias;
  bool hidden;
  bool obsolete;
  bool enterpriseOnly;

  // program options of the section
  std::map<std::string, Option> options;
  
  // sub-headlines
  std::map<std::string, std::string> headlines;
};

/// @brief section only available in enterprise builds
/// must have the same storage layout as struct Section
struct EnterpriseSection : public Section {
  EnterpriseSection(std::string const& name, std::string const& description,
                    std::string const& link, std::string const& alias, 
                    bool hidden, bool obsolete)
      : Section(name, description, link, alias, hidden, obsolete) {
    enterpriseOnly = true;
  }
};

}  // namespace options
}  // namespace arangodb

#endif
