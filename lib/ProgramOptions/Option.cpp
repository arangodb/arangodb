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

#include "Option.h"

#include "ProgramOptions/Parameters.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>

using namespace arangodb::options;

// create an option, consisting of single string
Option::Option(std::string const& value, std::string const& description,
               Parameter* parameter, std::underlying_type<Flags>::type flags)
    : section(), name(), description(description), shorthand(), parameter(parameter), flags(flags) {
  auto parts = splitName(value);
  section = parts.first;
  name = parts.second;

  size_t const pos = name.find(',');
  if (pos != std::string::npos) {
    shorthand = stripShorthand(name.substr(pos + 1));
    name = name.substr(0, pos);
  }
}

void Option::toVPack(VPackBuilder& builder) const {
  parameter->toVPack(builder);
}

// print help for an option
// the special search string "." will show help for all sections, even if hidden
void Option::printHelp(std::string const& search, size_t tw, size_t ow, bool) const {
  if (search == "." || !hasFlag(arangodb::options::Flags::Hidden)) {
    std::cout << "  " << pad(nameWithType(), ow) << "   ";

    std::string value = description;
    if (hasFlag(arangodb::options::Flags::Obsolete)) {
      value += " (obsolete option)";
    } else {
      std::string description = parameter->description();
      if (!description.empty()) {
        value.append(". ");
        value.append(description);
      }
      value += " (default: " + parameter->valueString() + ")";
    }
    auto parts = wordwrap(value, tw - ow - 6);
    size_t const n = parts.size();
    for (size_t i = 0; i < n; ++i) {
      std::cout << trim(parts[i]) << std::endl;
      if (i < n - 1) {
        std::cout << "  " << pad("", ow) << "   ";
      }
    }
  }
}

// determine the width of an option help string
size_t Option::optionsWidth() const {
  if (hasFlag(arangodb::options::Flags::Hidden)) {
    return 0;
  }

  return nameWithType().size();
}

// strip the "--" from a string
std::string Option::stripPrefix(std::string const& name) {
  size_t pos = name.find("--");
  if (pos == 0) {
    // strip initial "--"
    return name.substr(2);
  }
  return name;
}

// strip the "-" from a string
std::string Option::stripShorthand(std::string const& name) {
  size_t pos = name.find("-");
  if (pos == 0) {
    // strip initial "-"
    return name.substr(1);
  }
  return name;
}

// split an option name at the ".", if it exists
std::pair<std::string, std::string> Option::splitName(std::string name) {
  std::string section;
  name = stripPrefix(name);
  // split at "."
  size_t pos = name.find(".");
  if (pos == std::string::npos) {
    // global option
    section = "";
  } else {
    // section-specific option
    section = name.substr(0, pos);
    name = name.substr(pos + 1);
  }

  return std::make_pair(section, name);
}

std::vector<std::string> Option::wordwrap(std::string const& value, size_t size) {
  std::vector<std::string> result;
  std::string next = value;

  if (size > 0) {
    while (next.size() > size) {
      size_t m = next.find_last_of("., ", size - 1);

      if (m == std::string::npos || m < size / 2) {
        m = size;
      } else {
        m += 1;
      }

      result.emplace_back(next.substr(0, m));
      next = next.substr(m);
    }
  }

  result.emplace_back(next);

  return result;
}

// right-pad a string
std::string Option::pad(std::string const& value, size_t length) {
  size_t const valueLength = value.size();
  if (valueLength > length) {
    return value.substr(0, length);
  }
  if (valueLength == length) {
    return value;
  }
  return value + std::string(length - valueLength, ' ');
}

std::string Option::trim(std::string const& value) {
  size_t const pos = value.find_first_not_of(" \t\n\r");
  if (pos == std::string::npos) {
    return "";
  }
  return value.substr(pos);
}
