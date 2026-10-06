////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "ConfigOptionsProvider.h"

#include <cstdlib>
#include <filesystem>

#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/directories.h"
#include "Basics/exitcodes.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/IniFileParser.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Translator.h"

namespace arangodb {

namespace {

bool checkConfigFile(std::string const& name) {
  LOG_TOPIC("393e7", TRACE, Logger::CONFIG)
      << "checking config file '" << name << "'";

  std::error_code existsEc;
  if (std::filesystem::exists(name, existsEc)) {
    LOG_TOPIC("e6bd8", DEBUG, Logger::CONFIG)
        << "found config file '" << name << "'";
    return true;
  }
  if (existsEc) {
    // An error occurred while checking the file
    LOG_TOPIC("e6bd9", ERR, Logger::CONFIG)
        << "error checking config file '" << name
        << "': " << existsEc.message();
  } else {
    // File simply does not exist
    LOG_TOPIC("e6bda", DEBUG, Logger::CONFIG)
        << "config file '" << name << "' does not exist";
  }
  return false;
}

std::string findConfigFile(std::string const& basename,
                           std::vector<std::string> const& locations,
                           bool checkArangoImp) {
  std::string filename;
  for (auto const& location : locations) {
    auto name = basics::FileUtils::buildFilename(location, basename);
    if (checkConfigFile(name)) {
      return name;
    }

    if (checkArangoImp) {
      name = basics::FileUtils::buildFilename(location, "arangoimp.conf");
      if (checkConfigFile(name)) {
        return name;
      }
    }
  }
  return {};
}

void parseConfigFile(std::string const& filename,
                     std::shared_ptr<options::ProgramOptions> progOpts) {
  std::string local = filename + ".local";
  options::IniFileParser parser(progOpts.get());

  LOG_TOPIC("f6420", TRACE, Logger::CONFIG)
      << "checking override '" << local << "'";

  std::error_code pathEc;
  if (std::filesystem::is_regular_file(local, pathEc)) {
    LOG_TOPIC("3d2d0", DEBUG, Logger::CONFIG)
        << "loading override '" << local << "'";

    if (!parser.parse(local, true)) {
      FATAL_ERROR_EXIT_CODE(progOpts->processingResult().exitCodeOrFailure());
    }
  } else {
    LOG_TOPIC("d601e", TRACE, Logger::CONFIG) << "no override file found";
  }

  LOG_TOPIC("02398", DEBUG, Logger::CONFIG) << "loading '" << filename << "'";

  if (!parser.parse(filename, true)) {
    FATAL_ERROR_EXIT_CODE(progOpts->processingResult().exitCodeOrFailure());
  }
}

}  // namespace

using namespace arangodb::basics;
using namespace arangodb::options;

void ConfigOptionsProvider::declareOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options,
    ConfigFeatureOptions& opts) {
  options->addOption("--configuration,-c",
                     "The configuration file or \"none\".",
                     new StringParameter(&opts.file));

  options->addOption(
      "--config", "The configuration file or \"none\".",
      new StringParameter(&opts.file),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--define,-D",
      "Define a value for a `@key@` entry in the configuration file using the "
      "syntax `\"key=value\"`.",
      new VectorParameter<StringParameter>(&opts.defines),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--check-configuration", "Check the configuration and exit.",
      new BooleanParameter(&opts.checkConfiguration),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon,
                                          arangodb::options::Flags::Command));

  options->addOption(
      "--honor-nsswitch",
      "Allow hostname lookup configuration via /etc/nsswitch.conf if on "
      "Linux/glibc.",
      new BooleanParameter(&opts.honorNsswitch),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
}

void ConfigOptionsProvider::processOptionsImpl(
    std::shared_ptr<options::ProgramOptions> progOpts,
    ConfigFeatureOptions& configOpts) {
  for (auto const& def : configOpts.defines) {
    options::DefineEnvironment(def);
  }

  auto context = ArangoGlobalContext::CONTEXT;
  std::string const progname =
      context != nullptr ? context->binaryName()
                         : TRI_BinaryName(progOpts->progname().c_str());
  char const* binaryPath = progOpts->binaryPath();

  loadConfigFile(progOpts, progname, binaryPath, configOpts);

  if (configOpts.checkConfiguration) {
    exit(EXIT_SUCCESS);
  }
}

void ConfigOptionsProvider::loadConfigFile(
    std::shared_ptr<options::ProgramOptions> progOpts,
    std::string const& progname, char const* binaryPath,
    ConfigFeatureOptions& configOpts) {
  if (StringUtils::tolower(configOpts.file) == "none") {
    LOG_TOPIC("6cb22", DEBUG, Logger::CONFIG) << "using no config file at all";
    return;
  }

  // always prefer an explicitly given config file
  if (!configOpts.file.empty()) {
    std::error_code existsEc;
    if (!std::filesystem::exists(configOpts.file, existsEc)) {
      if (existsEc) {
        // An actual error occurred (permissions, I/O, invalid path, etc.
        LOG_TOPIC("f21fa", FATAL, Logger::CONFIG)
            << "error checking config file '" << configOpts.file
            << "': " << existsEc.message();
        FATAL_ERROR_EXIT_CODE(TRI_EXIT_CONFIG_NOT_FOUND);
      } else {
        // File simply does not exist
        LOG_TOPIC("f21f9", FATAL, Logger::CONFIG)
            << "cannot read config file '" << configOpts.file << "'";
        FATAL_ERROR_EXIT_CODE(TRI_EXIT_CONFIG_NOT_FOUND);
      }
    }

    parseConfigFile(configOpts.file, progOpts);
    return;
  }

  // check the following location in this order:
  //   ./etc/relative/<PRGNAME>.conf
  //   <PRGNAME>.conf
  //   ${HOME}/.arangodb/<PRGNAME>.conf
  //   /etc/arangodb/<PRGNAME>.conf
  //

  auto const& result = progOpts->processingResult();
  bool const versionRequested =
      result.touched("version") || result.touched("version-json");
  bool const fatal = !versionRequested;

  auto context = ArangoGlobalContext::CONTEXT;
  // the config file is looked up as <binary name>.conf
  std::string basename = progname;
  bool checkArangoImp = (progname == "arangoimport");

  if (!basename.ends_with(".conf")) {
    basename += ".conf";
  }

  std::vector<std::string> locations;
  locations.reserve(4);

  auto const current = std::filesystem::current_path().string();
  // ./etc/relative/ is always first choice, if it exists
  locations.emplace_back(FileUtils::buildFilename(current, "etc", "relative"));

  if (context != nullptr) {
    auto root = context->runRoot();
    // will resolve to ./build/etc/arangodb3/ in maintainer builds

    LOG_TOPIC("f39d1", TRACE, Logger::CONFIG)
        << "checking root location '" << root << "'";

    auto location = FileUtils::buildFilename(root, _SYSCONFDIR_);
    locations.emplace_back(location);
  }

  // ./
  locations.emplace_back(current);

  // ~/.arangodb/
  locations.emplace_back(
      FileUtils::buildFilename(FileUtils::homeDirectory(), ".arangodb"));
  locations.emplace_back(FileUtils::configDirectory(binaryPath));

  std::string filename = findConfigFile(basename, locations, checkArangoImp);
  if (filename.empty()) {
    LOG_TOPIC("f4964", DEBUG, Logger::CONFIG) << "cannot find any config file";
    if (fatal) {
      std::string locationMsg;
      for (auto const& it : locations) {
        if (!locationMsg.empty()) {
          locationMsg += ", ";
        }
        locationMsg += "'" + FileUtils::buildFilename(it, basename) + "'";
      }
      locationMsg = "(tried locations: " + locationMsg + ")";
      progOpts->failNotice(TRI_EXIT_CONFIG_NOT_FOUND,
                           "cannot find configuration file\n\n" + locationMsg);
      FATAL_ERROR_EXIT_CODE(progOpts->processingResult().exitCodeOrFailure());
    }
    return;
  }

  LOG_TOPIC("fc54e", DEBUG, Logger::CONFIG)
      << "found config file '" << filename << "'";
  parseConfigFile(filename, progOpts);
}

}  // namespace arangodb
