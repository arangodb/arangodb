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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ConfigFeature.h"

#include <iostream>

#include "ApplicationFeatures/VersionFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/directories.h"
#include "Logger/Logger.h"
#include "ProgramOptions/IniFileParser.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "ProgramOptions/Translator.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::options;

ConfigFeature::ConfigFeature(application_features::ApplicationServer* server,
                             std::string const& progname)
    : ApplicationFeature(server, "Config"),
      _file(""),
      _checkConfiguration(false),
      _progname(progname) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
  startsAfter("ShellColors");
}

void ConfigFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--configuration,-c", "the configuration file or 'none'",
                     new StringParameter(&_file));

  // add --config as an alias for --configuration. both point to the same
  // variable!
  options->addHiddenOption("--config", "the configuration file or 'none'",
                           new StringParameter(&_file));

  options->addHiddenOption("--define,-D",
                           "define key=value for a @key@ entry in config file",
                           new VectorParameter<StringParameter>(&_defines));

  options->addHiddenOption("--check-configuration",
                           "check the configuration and exit",
                           new BooleanParameter(&_checkConfiguration));
}

void ConfigFeature::loadOptions(std::shared_ptr<ProgramOptions> options,
                                char const* binaryPath) {
  for (auto const& def : _defines) {
    arangodb::options::DefineEnvironment(def);
  }

  loadConfigFile(options, _progname, binaryPath);

  if (_checkConfiguration) {
    exit(EXIT_SUCCESS);
  }
}

void ConfigFeature::loadConfigFile(std::shared_ptr<ProgramOptions> options,
                                   std::string const& progname,
                                   char const* binaryPath) {
  if (StringUtils::tolower(_file) == "none") {
    LOG_TOPIC(DEBUG, Logger::CONFIG) << "using no config file at all";
    return;
  }

  bool fatal = true;

  auto version = dynamic_cast<VersionFeature*>(
      application_features::ApplicationServer::lookupFeature("Version"));

  if (version != nullptr && version->printVersion()) {
    fatal = false;
  }

  // always prefer an explicitly given config file
  if (!_file.empty()) {
    if (!FileUtils::exists(_file)) {
      LOG_TOPIC(FATAL, Logger::CONFIG) << "cannot read config file '" << _file
                                       << "'";
      FATAL_ERROR_EXIT();
    }

    auto local = _file + ".local";

    IniFileParser parser(options.get());

    if (FileUtils::exists(local)) {
      LOG_TOPIC(DEBUG, Logger::CONFIG) << "loading override '" << local << "'";

      if (!parser.parse(local, true)) {
        FATAL_ERROR_EXIT();
      }
    }

    LOG_TOPIC(DEBUG, Logger::CONFIG) << "using user supplied config file '"
                                     << _file << "'";

    if (!parser.parse(_file, true)) {
      FATAL_ERROR_EXIT();
    }

    return;
  }

  // clang-format off
  //
  // check the following location in this order:
  //
  //   <PRGNAME>.conf
  //   ./etc/relative/<PRGNAME>.conf
  //   ${HOME}/.arangodb/<PRGNAME>.conf
  //   /etc/arangodb/<PRGNAME>.conf
  //
  // clang-format on

  auto context = ArangoGlobalContext::CONTEXT;
  std::string basename = progname;

  if (!StringUtils::isSuffix(basename, ".conf")) {
    basename += ".conf";
  }

  std::vector<std::string> locations;

  if (context != nullptr) {
    auto root = context->runRoot();
    auto location = FileUtils::buildFilename(root, _SYSCONFDIR_);

    LOG_TOPIC(TRACE, Logger::CONFIG) << "checking root location '" << root
                                     << "'";

    locations.emplace_back(location);
  }

  std::string current = FileUtils::currentDirectory().result();
  locations.emplace_back(current);
  locations.emplace_back(FileUtils::buildFilename(current, "etc", "relative"));
  locations.emplace_back(
      FileUtils::buildFilename(FileUtils::homeDirectory(), ".arangodb"));
  locations.emplace_back(FileUtils::configDirectory(binaryPath));

  std::string filename;

  for (auto const& location : locations) {
    auto name = FileUtils::buildFilename(location, basename);
    LOG_TOPIC(TRACE, Logger::CONFIG) << "checking config file '" << name << "'";

    if (FileUtils::exists(name)) {
      LOG_TOPIC(DEBUG, Logger::CONFIG) << "found config file '" << name << "'";
      filename = name;
      break;
    }
  }

  if (filename.empty()) {
    LOG_TOPIC(DEBUG, Logger::CONFIG) << "cannot find any config file";
  }

  IniFileParser parser(options.get());
  std::string local = filename + ".local";

  LOG_TOPIC(TRACE, Logger::CONFIG) << "checking override '" << local << "'";

  if (FileUtils::exists(local)) {
    LOG_TOPIC(DEBUG, Logger::CONFIG) << "loading override '" << local << "'";

    if (!parser.parse(local, true)) {
      FATAL_ERROR_EXIT();
    }
  } else {
    LOG_TOPIC(TRACE, Logger::CONFIG) << "no override file found";
  }

  LOG_TOPIC(DEBUG, Logger::CONFIG) << "loading '" << filename << "'";

  if (filename.empty()) {
    if (fatal) {
      size_t i = 0;
      std::string locationMsg = "(tried locations: ";
      for (auto const& it : locations) {
        if (i++ > 0) {
          locationMsg += ", ";
        }
        locationMsg += "'" + FileUtils::buildFilename(it, basename) + "'";
      }
      locationMsg += ")";
      options->failNotice("cannot find configuration file\n\n" + locationMsg);
      exit(EXIT_FAILURE);
    } else {
      return;
    }
  }

  if (!parser.parse(filename, true)) {
    exit(EXIT_FAILURE);
  }
}
