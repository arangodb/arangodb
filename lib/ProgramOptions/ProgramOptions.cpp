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

#include "ProgramOptions.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/files.h"
#include "Basics/levenshtein.h"
#include "Basics/terminal-utils.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Section.h"
#include "ProgramOptions/Translator.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <algorithm>
#include <iostream>
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#define ARANGODB_PROGRAM_OPTIONS_PROGNAME "#progname#"

using namespace arangodb::options;

ProgramOptions::ProgramOptions(char const* progname, std::string const& usage,
                               std::string const& more, char const* binaryPath)
    : _progname(progname),
      _usage(usage),
      _more(more),
      _similarity(TRI_Levenshtein),
      _processingResult(),
      _sealed(false),
      _overrideOptions(false),
      _binaryPath(binaryPath) {
  // find progname wildcard in string
  size_t const pos = _usage.find(ARANGODB_PROGRAM_OPTIONS_PROGNAME);

  if (pos != std::string::npos) {
    // and replace it with actual program name
    _usage = usage.substr(0, pos) + _progname +
             _usage.substr(pos + strlen(ARANGODB_PROGRAM_OPTIONS_PROGNAME));
  }

  _translator = EnvironmentTranslator;
}

// sets a value translator
void ProgramOptions::setTranslator(
    std::function<std::string(std::string const&, char const*)> const& translator) {
  _translator = translator;
}

// prints usage information
void ProgramOptions::printUsage() const {
  std::cout << _usage << std::endl << std::endl;
}

// prints a help for all options, or the options of a section
// the special search string "*" will show help for all sections
// the special search string "." will show help for all sections, even if
// hidden
void ProgramOptions::printHelp(std::string const& search) const {
  bool const colors = (isatty(STDOUT_FILENO) != 0);
  printUsage();

  TRI_TerminalSize ts = TRI_DefaultTerminalSize();
  size_t const tw = ts.columns;
  size_t const ow = optionsWidth();

  for (auto const& it : _sections) {
    if (search == "*" || search == "." || search == it.second.name) {
      it.second.printHelp(search, tw, ow, colors);
    }
  }

  if (search == "*") {
    printSectionsHelp();
  }
}

// prints the names for all section help options
void ProgramOptions::printSectionsHelp() const {
  char const* colorStart = "";
  char const* colorEnd = "";

  if (isatty(STDOUT_FILENO)) {
    colorStart = ShellColorsFeature::SHELL_COLOR_BRIGHT;
    colorEnd = ShellColorsFeature::SHELL_COLOR_RESET;
  }

  // print names of sections
  std::cout << _more;
  for (auto const& it : _sections) {
    if (!it.second.name.empty() && it.second.hasOptions()) {
      std::cout << "  " << colorStart << "--help-" << it.second.name << colorEnd;
    }
  }
  std::cout << std::endl;
}

// returns a VPack representation of the option values, with optional
// filters applied to filter out specific options. 
// the filter function is expected to return true
// for any options that should become part of the result
VPackBuilder ProgramOptions::toVPack(bool onlyTouched, bool detailed,
                                     std::function<bool(std::string const&)> const& filter) const {
  VPackBuilder builder;
  builder.openObject();

  walk(
      [this, &builder, &filter, &detailed](Section const& section, Option const& option) {
        std::string full(option.fullName());
        
        if (!filter(full)) {
          return;
        }

        // add key
        builder.add(VPackValue(full));

        // add value
        if (detailed) {
          builder.openObject();
          builder.add("section", VPackValue(option.section));
          builder.add("description", VPackValue(option.description));
          builder.add("category", VPackValue(option.hasFlag(arangodb::options::Flags::Command)
                                                 ? "command"
                                                 : "option"));
          builder.add("hidden", VPackValue(option.hasFlag(arangodb::options::Flags::Hidden)));
          builder.add("type", VPackValue(option.parameter->name()));
          builder.add("obsolete",
                      VPackValue(option.hasFlag(arangodb::options::Flags::Obsolete)));
          builder.add("enterpriseOnly",
                      VPackValue(section.enterpriseOnly ||
                                 option.hasFlag(arangodb::options::Flags::Enterprise)));
          builder.add("requiresValue", VPackValue(option.parameter->requiresValue()));
         
          // OS support
          builder.add("os", VPackValue(VPackValueType::Array));
          if (option.hasFlag(arangodb::options::Flags::OsLinux)) {
            builder.add(VPackValue("linux"));
          }
          if (option.hasFlag(arangodb::options::Flags::OsMac)) {
            builder.add(VPackValue("macos"));
          }
          if (option.hasFlag(arangodb::options::Flags::OsWindows)) {
            builder.add(VPackValue("windows"));
          }
          builder.close();
          
          // component support
          char const* arangod = "arangod";
          if (_progname.size() >= strlen(arangod) &&
              _progname.compare(_progname.size() - strlen(arangod), strlen(arangod), arangod) == 0) {
            builder.add("component", VPackValue(VPackValueType::Array));
            if (option.hasFlag(arangodb::options::Flags::OnCoordinator)) {
              builder.add(VPackValue("coordinator"));
            }
            if (option.hasFlag(arangodb::options::Flags::OnDBServer)) {
              builder.add(VPackValue("dbserver"));
            }
            if (option.hasFlag(arangodb::options::Flags::OnAgent)) {
              builder.add(VPackValue("agent"));
            }
            if (option.hasFlag(arangodb::options::Flags::OnSingle)) {
              builder.add(VPackValue("single"));
            }
            builder.close();
          }

          // version the option was introduced in (unknown for some older options)
          builder.add(VPackValue("introducedIn"));
          if (option.hasIntroducedIn()) {
            builder.openArray();
            for (auto const& it : option.introducedInVersions) {
              builder.add(VPackValue(option.toVersionString(it)));
            }
            builder.close();
          } else {
            builder.add(VPackValue(VPackValueType::Null));
          }

          // version the option was deprecated in (not set for still-active options)
          builder.add(VPackValue("deprecatedIn"));
          if (option.hasDeprecatedIn()) {
            builder.openArray();
            for (auto const& it : option.deprecatedInVersions) {
              builder.add(VPackValue(option.toVersionString(it)));
            }
            builder.close();
          } else {
            builder.add(VPackValue(VPackValueType::Null));
          }

          std::string values = option.parameter->description();
          if (!values.empty()) {
            builder.add("values", VPackValue(values));
          }

          if (!option.hasFlag(arangodb::options::Flags::Command)) {
            // command-like options are commands, thus they shouldn't have
            // a "default" value
            builder.add(VPackValue("default"));
            option.toVPack(builder);
          }
          builder.add("dynamic",
                      VPackValue(option.hasFlag(arangodb::options::Flags::Dynamic)));
          builder.close();
        } else {
          option.toVPack(builder);
        }
      },
      onlyTouched, false);

  builder.close();
  return builder;
}

// translate a shorthand option
std::string ProgramOptions::translateShorthand(std::string const& name) const {
  auto it = _shorthands.find(name);

  if (it == _shorthands.end()) {
    return name;
  }
  return (*it).second;
}

void ProgramOptions::walk(std::function<void(Section const&, Option const&)> const& callback,
                          bool onlyTouched, bool includeObsolete) const {
  for (auto const& it : _sections) {
    if (!includeObsolete && it.second.obsolete) {
      // obsolete section. ignore it
      continue;
    }
    for (auto const& it2 : it.second.options) {
      if (!includeObsolete && it2.second.hasFlag(arangodb::options::Flags::Obsolete)) {
        // obsolete option. ignore it
        continue;
      }
      if (onlyTouched && !_processingResult.touched(it2.second.fullName())) {
        // option not touched. skip over it
        continue;
      }
      callback(it.second, it2.second);
    }
  }
}

// checks whether a specific option exists
// if the option does not exist, this will flag an error
bool ProgramOptions::require(std::string const& name) {
  std::string const& modernized = modernize(name);

  auto parts = Option::splitName(modernized);
  auto it = _sections.find(parts.first);

  if (it == _sections.end()) {
    return unknownOption(modernized);
  }

  auto it2 = (*it).second.options.find(parts.second);

  if (it2 == (*it).second.options.end()) {
    return unknownOption(modernized);
  }

  return true;
}

// sets a value for an option
bool ProgramOptions::setValue(std::string const& name, std::string const& value) {
  std::string const& modernized = modernize(name);

  if (!_overrideOptions && _processingResult.frozen(modernized)) {
    // option already frozen. don't override it
    return true;
  }

  auto parts = Option::splitName(modernized);
  auto it = _sections.find(parts.first);

  if (it == _sections.end()) {
    return unknownOption(modernized);
  }

  if ((*it).second.obsolete) {
    // section is obsolete. ignore it
    return true;
  }

  auto it2 = (*it).second.options.find(parts.second);

  if (it2 == (*it).second.options.end()) {
    return unknownOption(modernized);
  }

  auto& option = (*it2).second;
  if (option.hasFlag(arangodb::options::Flags::Obsolete)) {
    // option is obsolete. ignore it
    _processingResult.touch(modernized);
    return true;
  }

  if (option.hasFlag(options::Flags::FlushOnFirst) &&
      _alreadyFlushed.find(parts.second) == _alreadyFlushed.end()) {
    _alreadyFlushed.insert(parts.second);
    option.parameter->flushValue();
  }
  std::string result = option.parameter->set(_translator(value, _binaryPath));

  if (!result.empty()) {
    // parameter validation failed
    char const* colorStart1 = "";
    char const* colorStart2 = "";
    char const* colorEnd = "";

    if (isatty(STDERR_FILENO)) {
      colorStart1 = ShellColorsFeature::SHELL_COLOR_RED;
      colorStart2 = ShellColorsFeature::SHELL_COLOR_BOLD_RED;
      colorEnd = ShellColorsFeature::SHELL_COLOR_RESET;
    }
    return fail(std::string("error setting value for option '") + colorStart2 + "--" + modernized + colorEnd + "': " + colorStart1 + result + colorEnd);
  }

  _processingResult.touch(modernized);

  return true;
}

// finalizes a pass, copying touched into frozen
void ProgramOptions::endPass() {
  if (_overrideOptions) {
    return;
  }
  for (auto const& it : _processingResult._touched) {
    _processingResult.freeze(it);
  }
}

std::unordered_map<std::string, std::string> ProgramOptions::modernizedOptions() const {
  std::unordered_map<std::string, std::string> result;
  for (auto const& name : _alreadyModernized) {
    auto it = _oldOptions.find(name);
    if (it != _oldOptions.end()) {
      result.emplace(name, (*it).second);
    }
  }
  return result;
}
  
// sets a single old option and its replacement name
void ProgramOptions::addOldOption(std::string const& old, std::string const& replacement) {
  _oldOptions[Option::stripPrefix(old)] = Option::stripPrefix(replacement);
}

// check whether or not an option requires a value
bool ProgramOptions::requiresValue(std::string const& name) {
  std::string const& modernized = modernize(name);

  auto parts = Option::splitName(modernized);
  auto it = _sections.find(parts.first);

  if (it == _sections.end()) {
    return false;
  }

  auto it2 = (*it).second.options.find(parts.second);

  if (it2 == (*it).second.options.end()) {
    return false;
  }

  return (*it2).second.parameter->requiresValue();
}

// returns the option by name. will throw if the option cannot be found
Option& ProgramOptions::getOption(std::string const& name) {
  std::string const& modernized = modernize(name);

  std::string stripped = modernized;
  size_t const pos = stripped.find(',');
  if (pos != std::string::npos) {
    // remove shorthand
    stripped = stripped.substr(0, pos);
  }
  auto parts = Option::splitName(stripped);
  auto it = _sections.find(parts.first);

  if (it == _sections.end()) {
    throw std::logic_error(std::string("option '") + stripped + "' not found");
  }

  auto it2 = (*it).second.options.find(parts.second);

  if (it2 == (*it).second.options.end()) {
    throw std::logic_error(std::string("option '") + stripped + "' not found");
  }

  return (*it2).second;
}

// returns an option description
std::string ProgramOptions::getDescription(std::string const& name) {
  std::string const& modernized = modernize(name);

  auto parts = Option::splitName(modernized);
  auto it = _sections.find(parts.first);

  if (it == _sections.end()) {
    return "";
  }

  auto it2 = (*it).second.options.find(parts.second);

  if (it2 == (*it).second.options.end()) {
    return "";
  }

  return (*it2).second.description;
}

// handle an unknown option
bool ProgramOptions::unknownOption(std::string const& name) {
  char const* colorStart1 = "";
  char const* colorStart2 = "";
  char const* colorStart3 = "";
  char const* colorEnd = "";

  if (isatty(STDERR_FILENO)) {
    colorStart1 = ShellColorsFeature::SHELL_COLOR_RED;
    colorStart2 = ShellColorsFeature::SHELL_COLOR_BOLD_RED;
    colorStart3 = ShellColorsFeature::SHELL_COLOR_BRIGHT;
    colorEnd = ShellColorsFeature::SHELL_COLOR_RESET;
  }

  fail(std::string(colorStart1) + "unknown option '" + colorStart2 + "--" + name + colorStart1 + "'" + colorEnd);

  auto similarOptions = similar(name, 8, 4);
  if (!similarOptions.empty()) {
    if (similarOptions.size() == 1) {
      std::cerr << "Did you mean this?" << std::endl;
    } else {
      std::cerr << "Did you mean one of these?" << std::endl;
    }
    // determine maximum width
    size_t maxWidth = 0;
    for (auto const& it : similarOptions) {
      maxWidth = (std::max)(maxWidth, it.size());
    }

    for (auto const& it : similarOptions) {
      std::cerr << "  " << colorStart3 << Option::pad(it, maxWidth) << colorEnd
                << "    " << getDescription(it) << std::endl;
    }
    std::cerr << std::endl;
  }

  std::cerr << "Use " << colorStart3 << "--help" << colorEnd << " or "
            << colorStart3 << "--help-all" << colorEnd
            << " to get an overview of available options" << std::endl
            << std::endl;

  return false;
}

// report an error (callback from parser)
bool ProgramOptions::fail(std::string const& message) {
  _processingResult.failed(true);
  
  char const* colorStart = "";
  char const* colorEnd = "";

  if (isatty(STDERR_FILENO)) {
    colorStart = ShellColorsFeature::SHELL_COLOR_RED;
    colorEnd = ShellColorsFeature::SHELL_COLOR_RESET;
  }
  std::cerr << colorStart << "Error while processing " << _context << " for "
            << TRI_Basename(_progname.c_str()) << ":" << colorEnd << std::endl;
  failNotice(message);
  std::cerr << std::endl;
#ifdef _WIN32
  // additionally log these errors to the debug output window in MSVC so
  // we can see them during development
  OutputDebugString(message.c_str());
  OutputDebugString("\r\n");
#endif
  return false;
}

void ProgramOptions::failNotice(std::string const& message) {
  _processingResult.failed(true);
  std::cerr << "  " << message << std::endl;
#ifdef _WIN32
  // additionally log these errors to the debug output window in MSVC so
  // we can see them during development
  OutputDebugString(message.c_str());
  OutputDebugString("\r\n");
#endif
}

// add a positional argument (callback from parser)
void ProgramOptions::addPositional(std::string const& value) {
  _processingResult._positionals.emplace_back(value);
}

// adds an option to the list of options
void ProgramOptions::addOption(Option const& option) {
  checkIfSealed();
  std::map<std::string, Section>::iterator sectionIt = addSection(option.section, "");

  if (!option.shorthand.empty()) {
    if (!_shorthands.try_emplace(option.shorthand, option.fullName()).second) {
      throw std::logic_error(
          std::string("shorthand option already defined for option ") + option.displayName());
    }
  }

  Section& section = (*sectionIt).second;
  section.options.try_emplace(option.name, option);
}
 
// modernize an option name
std::string const& ProgramOptions::modernize(std::string const& name) {
  auto it = _oldOptions.find(Option::stripPrefix(name));
  if (it == _oldOptions.end()) {
    return name;
  }

  // note which old options have been used
  _alreadyModernized.emplace(name);

  return (*it).second;
}

// determine maximum width of all options labels
size_t ProgramOptions::optionsWidth() const {
  size_t width = 0;
  for (auto const& it : _sections) {
    width = (std::max)(width, it.second.optionsWidth());
  }
  return width;
}

// check if the options are already sealed and throw if yes
void ProgramOptions::checkIfSealed() const {
  if (_sealed) {
    throw std::logic_error("program options are already sealed");
  }
}

// get a list of similar options
std::vector<std::string> ProgramOptions::similar(std::string const& value,
                                                 int cutOff, size_t maxResults) {
  std::vector<std::string> result;

  if (_similarity != nullptr) {
    // build a sorted map of similar values first
    std::multimap<int, std::string> distances;
    // walk over all options
    walk(
        [this, &value, &distances](Section const&, Option const& option) {
          if (option.fullName() != value) {
            distances.emplace(_similarity(value, option.fullName()), option.displayName());
          }
        },
        false);

    // now return the ones that have an edit distance not higher than the
    // cutOff value
    int last = 0;
    for (auto const& it : distances) {
      if (last > 1 && it.first > 2 * last) {
        break;
      }
      if (it.first > cutOff && it.second.substr(0, value.size()) != value) {
        continue;
      }
      result.emplace_back(it.second);
      if (result.size() >= maxResults) {
        break;
      }
      last = it.first;
    }
  }

  if (value.size() >= 3) {
    // additionally add all options that have the search string as part
    // of their name
    walk(
        [&value, &result](Section const&, Option const& option) {
          if (option.fullName().find(value) != std::string::npos) {
            result.emplace_back(option.displayName());
          }
        },
        false);
  }

  // produce a unique result
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());

  return result;
}
