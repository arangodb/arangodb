////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "ProgramOptions/Section.h"

#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aragndob::velocypack {
class Builder;
}

namespace arangodb::options {
struct Option;
struct Parameter;

// program options data structure
// typically an application will have a single instance of this
class ProgramOptions {
 public:
  // filter function to hide certain options in the outputs of
  // - JavaScript options api: require("internal").options()
  // - HTTP REST API: GET /_admin/options
  // filter function returns false for any option to be filtered out,
  // and true for all options to include in the output.
  static std::function<bool(std::string const&)> const defaultOptionsFilter;

  // struct containing the option processing result
  class ProcessingResult {
   public:
    ProcessingResult();
    ~ProcessingResult();

    // mark an option as being touched during options processing
    void touch(std::string const& name);

    // whether or not an option was touched during options processing,
    // including the current pass
    bool touched(std::string const& name) const;

    // mark an option as being frozen
    void freeze(std::string const& name);

    // whether or not an option was touched during options processing,
    // not including the current pass
    bool frozen(std::string const& name) const;

    // mark options processing as failed
    void fail(int exitCode) noexcept;

    // return the registered exit code
    int exitCode() const noexcept;

    int exitCodeOrFailure() const noexcept;

    // values of all positional arguments found
    std::vector<std::string> _positionals;

    // which options were touched during option processing
    // this includes options that are touched in the current pass
    std::unordered_set<std::string> _touched;

    // which options were touched during option processing
    // this does not include options that are touched in the current pass
    std::unordered_set<std::string> _frozen;

    // registered exit code (== 0 means ok, != 0 means error)
    int _exitCode;
  };

  // function type for determining the similarity between two strings
  typedef std::function<int(std::string const&, std::string const&)>
      SimilarityFuncType;

  // no need to copy this
  ProgramOptions(ProgramOptions const&) = delete;
  ProgramOptions& operator=(ProgramOptions const&) = delete;

  ProgramOptions(char const* progname, std::string const& usage,
                 std::string const& more, char const* binaryPath);

  std::string progname() const;

  // sets a value translator
  void setTranslator(std::function<std::string(std::string const&,
                                               char const*)> const& translator);

  // return a const reference to the processing result
  ProcessingResult const& processingResult() const;

  // return a reference to the processing result
  ProcessingResult& processingResult();

  // seal the options
  // trying to add an option or a section after sealing will throw an error
  void seal();

  // allow or disallow overriding already set options
  void allowOverride(bool value);

  bool allowOverride() const;

  // set context for error reporting
  void setContext(std::string const& value);

  // sets a single old option and its replacement name.
  // to be used when an option is renamed to still support the original name
  void addOldOption(std::string const& old, std::string const& replacement);

  // adds a section to the options
  std::map<std::string, Section>::iterator addSection(Section&& section);

  // adds a (regular) section to the program options
  auto addSection(std::string const& name, std::string const& description,
                  std::string const& link = "", bool hidden = false,
                  bool obsolete = false) {
    return addSection(Section(name, description, link, "", hidden, obsolete));
  }

  // adds an enterprise-only section to the program options
  auto addEnterpriseSection(std::string const& name,
                            std::string const& description,
                            std::string const& link = "", bool hidden = false,
                            bool obsolete = false) {
    return addSection(
        EnterpriseSection(name, description, link, "", hidden, obsolete));
  }

  // adds an option to the program options.
  Option& addOption(
      std::string const& name, std::string const& description,
      std::unique_ptr<Parameter> parameter,
      std::underlying_type<Flags>::type flags = makeFlags(Flags::Default));

  // adds an option to the program options. old API!
  Option& addOption(
      std::string const& name, std::string const& description,
      Parameter* parameter,
      std::underlying_type<Flags>::type flags = makeFlags(Flags::Default));

  // adds a deprecated option that has no effect to the program options to not
  // throw an unrecognized startup option error after upgrades until fully
  // removed. not listed by --help (uncommon option)
  Option& addObsoleteOption(std::string const& name,
                            std::string const& description, bool requiresValue);

  // adds a sub-headline for one option or a group of options
  void addHeadline(std::string const& prefix, std::string const& description);

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
  velocypack::Builder toVelocyPack(
      bool onlyTouched, bool detailed,
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
  bool requiresValue(std::string const& name);

  // returns the option by name. will throw if the option cannot be found
  Option& getOption(std::string const& name);

  // returns a pointer to an option value, specified by option name
  // returns a nullptr if the option is unknown
  template<typename T>
  T* get(std::string const& name) {
    return dynamic_cast<T*>(getParameter(name));
  }

  // returns an option description
  std::string getDescription(std::string const& name);

  // handle an unknown option
  void unknownOption(std::string const& name);

  // report an error (callback from parser)
  void fail(int exitCode, std::string const& message);

  void failNotice(int exitCode, std::string const& message);

  // add a positional argument (callback from parser)
  void addPositional(std::string const& value);

  // return all auto-modernized options
  std::unordered_map<std::string, std::string> modernizedOptions() const;

 private:
  // returns a pointer to an option value, specified by option name
  // returns a nullptr if the option is unknown
  Parameter* getParameter(std::string const& name) const;

  // adds an option to the list of options
  void addOption(Option&& option);

  // modernize an option name
  std::string const& modernize(std::string const& name);

  // determine maximum width of all options labels
  size_t optionsWidth() const;

  // check if the options are already sealed and throw if yes
  void checkIfSealed() const;

  // get a list of similar options
  std::vector<std::string> similar(std::string const& value, int cutOff,
                                   size_t maxResults);

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
  // already warned-about old, but modernized options
  std::unordered_set<std::string> _alreadyModernized;
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

}  // namespace arangodb::options
