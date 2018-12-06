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

#ifndef ARANGODB_PROGRAM_OPTIONS_OPTION_H
#define ARANGODB_PROGRAM_OPTIONS_OPTION_H 1

#include "Basics/Common.h"
#include "ProgramOptions/Parameters.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace options {

/// @brief option flags. these can be bit-ORed to combine multiple flags
enum class Flags : uint8_t {
  Normal = 0, // nothing special here
  Hidden = 1, // the option is hidden by default, only made visible by --help-all or --help-.
  Obsolete = 2, // the option is obsolete. setting it does not influence the program behavio
  Enterprise = 4, // the option is only available in the Enterprise edition 
  Command = 8, // the option executes a special command, e.g. --version, --check-configuration, --dump-options
  Dynamic = 16, // the option's default value is dynamic and depends on the target host configuration
};

static constexpr inline std::underlying_type<Flags>::type makeFlags() {
  return static_cast<std::underlying_type<Flags>::type>(Flags::Normal);
}
  
/// @brief helper for building flags
template <typename... Args>
static constexpr inline std::underlying_type<Flags>::type makeFlags(Flags flag, Args... args) {
  return static_cast<std::underlying_type<Flags>::type>(flag) + makeFlags(args...);
}

struct Parameter;

// a single program option container
struct Option {
  // options are default copy-constructible and default movable

  // create an option, consisting of single string
  Option(std::string const& value, std::string const& description,
         Parameter* parameter, std::underlying_type<Flags>::type flags);

  void toVPack(arangodb::velocypack::Builder& builder) const;

  bool hasFlag(Flags flag) const {
    return ((static_cast<std::underlying_type<Flags>::type>(flag) & flags) != 0);
  }

  // get display name for the option
  std::string displayName() const { return "--" + fullName(); }

  // get full name for the option
  std::string fullName() const {
    if (section.empty()) {
      return name;
    }
    return section + '.' + name;
  }

  // print help for an option
  // the special search string "." will show help for all sections, even if hidden
  void printHelp(std::string const& search, size_t tw, size_t ow, bool) const;

  std::string nameWithType() const {
    return displayName() + " " + parameter->typeDescription();
  }

  // determine the width of an option help string
  size_t optionsWidth() const;

  // strip the "--" from a string
  static std::string stripPrefix(std::string const& name);

  // strip the "-" from a string
  static std::string stripShorthand(std::string const& name);

  // split an option name at the ".", if it exists
  static std::pair<std::string, std::string> splitName(std::string name);

  static std::vector<std::string> wordwrap(std::string const& value,
                                           size_t size);

  // right-pad a string
  static std::string pad(std::string const& value, size_t length);

  static std::string trim(std::string const& value);

  std::string section;
  std::string name;
  std::string description;
  std::string shorthand;
  std::shared_ptr<Parameter> parameter;
  
  /// @brief option flags
  std::underlying_type<Flags>::type const flags;
};

}
}

#endif
