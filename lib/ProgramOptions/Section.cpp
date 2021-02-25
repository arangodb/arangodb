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

#include "Section.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ProgramOptions/Option.h"

#include <iostream>

using namespace arangodb::options;

// whether or not the section has (displayable) options
bool Section::hasOptions() const {
  if (!hidden) {
    for (auto const& it : options) {
      if (!it.second.hasFlag(arangodb::options::Flags::Hidden)) {
        return true;
      }
    }
  }
  return false;
}

// print help for a section
// the special search string "." will show help for all sections, even if hidden
void Section::printHelp(std::string const& search, size_t tw, size_t ow, bool colors) const {
  if (search != "." && (hidden || !hasOptions())) {
    return;
  }

  std::cout 
    << "Section '" 
    << (colors ? ShellColorsFeature::SHELL_COLOR_BRIGHT : "")
    << displayName() 
    << (colors ? ShellColorsFeature::SHELL_COLOR_RESET : "")
    << "' (" << description << ")";

  if (!link.empty()) {
    std::cout << " [";
    if (colors) {
      std::cout 
        << ShellColorsFeature::SHELL_COLOR_LINK_START << link 
        << ShellColorsFeature::SHELL_COLOR_LINK_MIDDLE << link 
        << ShellColorsFeature::SHELL_COLOR_LINK_END;
    } else {
      std::cout << link;
    }
    std::cout << "]";
  }
  std::cout << std::endl;

  auto hl = headlines.begin();

  // propagate print command to options
  for (auto const& it : options) {
    if (hl != headlines.end() && it.first >= (*hl).first) {
      // must print a headline
      std::cout << " # " << (*hl).second << std::endl;
      ++hl;
    }

    // print help for option
    it.second.printHelp(search, tw, ow, colors);
  }

  std::cout << std::endl;
}
  
// determine display width for a section
size_t Section::optionsWidth() const {
  size_t width = 0;

  if (!hidden) {
    for (auto const& it : options) {
      width = (std::max)(width, it.second.optionsWidth());
    }
  }

  return width;
}
