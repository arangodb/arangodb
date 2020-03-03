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

#ifndef ARANGODB_PROGRAM_OPTIONS_PROGRAM_OPTIONS_H
#define ARANGODB_PROGRAM_OPTIONS_PROGRAM_OPTIONS_H 1

#include "Basics/Common.h"

#include "ProgramOptions/Option.h"
#include "ProgramOptions/Section.h"


namespace arangodb {
namespace velocypack {
class Builder;
}
namespace options {

// program options data structure
// typically an application will have a single instance of this
class ProgramOptions {
 public:
  // struct containing the option processing result
  class ProcessingResult {
   public:
    ProcessingResult()
        : _positionals(), _touched(), _frozen(), _failed(false) {}
    ~ProcessingResult() = default;

    // mark an option as being touched during options processing
    void touch(std::string const& name) { _touched.emplace(name); }

    // whether or not an option was touched during options processing,
    // including the current pass
    bool touched(std::string const& name) const {
      return _touched.find(Option::stripPrefix(name)) != _touched.end();
    }

    // mark an option as being frozen
    void freeze(std::string const& name) { _frozen.emplace(name); }

    // whether or not an option was touched during options processing,
    // not including the current pass
    bool frozen(std::string const& name) const {
      return _frozen.find(Option::stripPrefix(name)) != _frozen.end();
    }

    // mark options processing as failed
    void failed(bool value) { _failed = value; }

    // whether or not options processing has failed
    bool failed() const { return _failed; }

    // values of all positional arguments found
    std::vector<std::string> _positionals;

    // which options were touched during option processing
    // this includes options that are touched in the current pass
    std::unordered_set<std::string> _touched;

    // which options were touched during option processing
    // this does not include options that are touched in the current pass
    std::unordered_set<std::string> _frozen;

    // whether or not options processing failed
    bool _failed;
  };

  // function type for determining the similarity between two strings
  typedef std::function<int(std::string const&, std::string const&)> SimilarityFuncType;

  // no need to copy this
  ProgramOptions(ProgramOptions const&) = delete;
  ProgramOptions& operator=(ProgramOptions const&) = delete;

  ProgramOptions(char const* progname, std::string const& usage,
                 std::string const& more, char const* binaryPath);

  // sets a value translator
  void setTranslator(std::function<std::string(std::string const&, char const*)> const& translator);

  // return a const reference to the processing result
  ProcessingResult const& processingResult() const { return _processingResult; }

  // return a reference to the processing result
  ProcessingResult& processingResult() { return _processingResult; }

  // seal the options
  // trying to add an option or a section after sealing will throw an error
  void seal() { _sealed = true; }

  // allow or disallow overriding already set options
  void allowOverride(bool value) {
    checkIfSealed();
    _overrideOptions = value;
  }

  bool allowOverride() const { return _overrideOptions; }

  // set context for error reporting
  void setContext(std::string const& value) { _context = value; }

  // sets a single old option and its replacement name
  void addOldOption(std::string const& old, std::string const& replacement) {
    _oldOptions[Option::stripPrefix(old)] = replacement;
  }

  // adds a section to the options
  auto addSection(Section const& section) {
    checkIfSealed();

    auto [it, emplaced] = _sections.try_emplace(section.name, section);
    if (!emplaced) {
      // section already present. check if we need to update it
      Section& sec = it->second;
      if (!section.description.empty() && sec.description.empty()) {
        // copy over description
        sec.description = section.description;
      }
    }
    return it;
  }

  // adds a (regular) section to the program options
  auto addSection(std::string const& name, std::string const& description) {
    return addSection(Section(name, description, "", false, false));
  }

  // adds an enterprise-only section to the program options
  auto addEnterpriseSection(std::string const& name, std::string const& description) {
    return addSection(EnterpriseSection(name, description, "", false, false));
  }

  // adds an option to the program options
  Option& addOption(std::string const& name, std::string const& description,
                    Parameter* parameter,
                    std::underlying_type<Flags>::type flags = makeFlags(Flags::Default)) {
    addOption(Option(name, description, parameter, flags));
    return getOption(name);
  }

  // adds an obsolete and hidden option to the program options
  Option& addObsoleteOption(std::string const& name,
                            std::string const& description, bool requiresValue) {
    addOption(Option(name, description, new ObsoleteParameter(requiresValue),
                     makeFlags(Flags::Hidden, Flags::Obsolete)));
    return getOption(name);
  }

  // prints usage information
  void printUsage() const;

  // prints a help for all options, or the options of a section
  // the special search string "*" will show help for all sections
  // the special search string "." will show help for all sections, even if
  // hidden
  void printHelp(std::string const& search) const;

  // prints the names for all section help options
  void printSectionsHelp() const;

  // returns a VPack representation of the option values, with optional
  // filters applied to filter out specific options.
  // the filter function is expected to return true
  // for any options that should become part of the result
  arangodb::velocypack::Builder toVPack(bool onlyTouched, bool detailed,
                                        std::function<bool(std::string const&)> const& filter) const;

  // translate a shorthand option
  std::string translateShorthand(std::string const& name) const;

  void walk(std::function<void(Section const&, Option const&)> const& callback,
            bool onlyTouched, bool includeObsolete = false) const;

  // checks whether a specific option exists
  // if the option does not exist, this will flag an error
  bool require(std::string const& name);

  // sets a value for an option
  bool setValue(std::string const& name, std::string const& value);

  // finalizes a pass, copying touched into frozen
  void endPass();

  // check whether or not an option requires a value
  bool requiresValue(std::string const& name) const;

  // returns the option by name. will throw if the option cannot be found
  Option& getOption(std::string const& name);

  // returns a pointer to an option value, specified by option name
  // returns a nullptr if the option is unknown
  template <typename T>
  T* get(std::string const& name) {
    auto parts = Option::splitName(name);
    auto it = _sections.find(parts.first);

    if (it == _sections.end()) {
      return nullptr;
    }

    auto it2 = (*it).second.options.find(parts.second);

    if (it2 == (*it).second.options.end()) {
      return nullptr;
    }

    Option& option = (*it2).second;

    return dynamic_cast<T*>(option.parameter.get());
  }

  // returns an option description
  std::string getDescription(std::string const& name);

  // handle an unknown option
  bool unknownOption(std::string const& name);

  // report an error (callback from parser)
  bool fail(std::string const& message);

  void failNotice(std::string const& message);

  // add a positional argument (callback from parser)
  void addPositional(std::string const& value);

 private:
  // adds an option to the list of options
  void addOption(Option const& option);

  // determine maximum width of all options labels
  size_t optionsWidth() const;

  // check if the options are already sealed and throw if yes
  void checkIfSealed() const;

  // get a list of similar options
  std::vector<std::string> similar(std::string const& value, int cutOff, size_t maxResults);

 private:
  // name of binary (i.e. argv[0])
  std::string _progname;
  // usage hint, e.g. "usage: #progname# [<options>] ..."
  std::string _usage;
  // help text for section help, e.g. "for more information use"
  std::string _more;
  // context string that's shown when errors are printed
  std::string _context;
  // already seen to flush program options
  std::unordered_set<std::string> _alreadyFlushed;
  // all sections
  std::map<std::string, Section> _sections;
  // shorthands for options, translating from short options to long option names
  // e.g. "-c" to "--configuration"
  std::unordered_map<std::string, std::string> _shorthands;
  // map with old options and their new equivalents, used for printing more
  // meaningful error messages when an invalid (but once valid) option was used
  std::unordered_map<std::string, std::string> _oldOptions;
  // callback function for determining the similarity between two option names
  SimilarityFuncType _similarity;
  // option processing result
  ProcessingResult _processingResult;
  // whether or not the program options setup is still mutable
  bool _sealed;
  // allow or disallow overriding already set options
  bool _overrideOptions;
  // translate input values
  std::function<std::string(std::string const&, char const*)> _translator;
  // directory of this binary
  char const* _binaryPath;
};
}  // namespace options
}  // namespace arangodb

#endif
