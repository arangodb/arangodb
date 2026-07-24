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
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/IniFileParser.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Translator.h"

namespace arangodb {

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
  char const* binaryPath = progOpts->binaryPath();
  auto const& result = progOpts->processingResult();
  bool const versionRequested =
      result.touched("version") || result.touched("verson-json");

  if (configOpts.progname.empty() && ArangoGlobalContext::CONTEXT != nullptr) {
    configOpts.progname = ArangoGlobalContext::CONTEXT->binaryName();
  }

  for (auto const& def : configOpts.defines) {
    options::DefineEnvironment(def);
  }

  if (configOpts.progname.empty()) {
    configOpts.progname = std::string{progOpts->progname()};
  }

  if (StringUtils::tolower(configOpts.file) == "none") {
    LOG_TOPIC("6cb22", DEBUG, Logger::CONFIG) << "using no config file at all";
  } else if (!configOpts.file.empty()) {
    // always prefer an explicitly given config file
    std::error_code existsEc;
    if (!std::filesystem::exists(configOpts.file, existsEc)) {
      if (existsEc) {
        LOG_TOPIC("f21fa", FATAL, Logger::CONFIG)
            << "error checking config file '" << configOpts.file
            << "': " << existsEc.message();
        FATAL_ERROR_EXIT_CODE(TRI_EXIT_CONFIG_NOT_FOUND);
      } else {
        LOG_TOPIC("f21f9", FATAL, Logger::CONFIG)
            << "cannot read config file '" << configOpts.file << "'";
        FATAL_ERROR_EXIT_CODE(TRI_EXIT_CONFIG_NOT_FOUND);
      }
    }

    auto local = configOpts.file + ".local";

    IniFileParser parser(progOpts.get());

    std::error_code pathEc;
    if (std::filesystem::is_regular_file(local, pathEc)) {
      LOG_TOPIC("9b20a", DEBUG, Logger::CONFIG)
          << "loading override '" << local << "'";

      if (!parser.parse(local, true)) {
        FATAL_ERROR_EXIT_CODE(progOpts->processingResult().exitCodeOrFailure());
      }
    }

    LOG_TOPIC("637c7", DEBUG, Logger::CONFIG)
        << "using user supplied config file '" << configOpts.file << "'";

    if (!parser.parse(configOpts.file, true)) {
      FATAL_ERROR_EXIT_CODE(progOpts->processingResult().exitCodeOrFailure());
    }
  } else {
    // clang-format off
    //
    // check the following location in this order:
    //
    //   ./etc/relative/<PRGNAME>.conf
    //   <PRGNAME>.conf
    //   ${HOME}/.arangodb/<PRGNAME>.conf
    //   /etc/arangodb/<PRGNAME>.conf
    //
    // clang-format on

    bool const fatal = !versionRequested;

    auto context = ArangoGlobalContext::CONTEXT;
    std::string basename = configOpts.progname;
    bool checkArangoImp = (configOpts.progname == "arangoimport");

    if (!basename.ends_with(".conf")) {
      basename += ".conf";
    }

    std::vector<std::string> locations;
    locations.reserve(4);

    auto const current = std::filesystem::current_path().string();
    locations.emplace_back(
        FileUtils::buildFilename(current, "etc", "relative"));

    if (context != nullptr) {
      auto root = context->runRoot();
      auto location = FileUtils::buildFilename(root, _SYSCONFDIR_);

      LOG_TOPIC("f39d1", TRACE, Logger::CONFIG)
          << "checking root location '" << root << "'";

      locations.emplace_back(location);
    }

    locations.emplace_back(current);

    locations.emplace_back(
        FileUtils::buildFilename(FileUtils::homeDirectory(), ".arangodb"));
    locations.emplace_back(FileUtils::configDirectory(binaryPath));

    std::string filename;

    for (auto const& location : locations) {
      auto name = FileUtils::buildFilename(location, basename);
      LOG_TOPIC("393e7", TRACE, Logger::CONFIG)
          << "checking config file '" << name << "'";

      std::error_code existsEc;
      if (std::filesystem::exists(name, existsEc)) {
        LOG_TOPIC("e6bd8", DEBUG, Logger::CONFIG)
            << "found config file '" << name << "'";
        filename = name;
        break;
      } else if (existsEc) {
        LOG_TOPIC("e6bd9", ERR, Logger::CONFIG)
            << "error checking config file '" << name
            << "': " << existsEc.message();
      } else {
        LOG_TOPIC("e6bda", DEBUG, Logger::CONFIG)
            << "config file '" << name << "' does not exist";
      }

      if (checkArangoImp) {
        name = FileUtils::buildFilename(location, "arangoimp.conf");
        LOG_TOPIC("b629e", TRACE, Logger::CONFIG)
            << "checking config file '" << name << "'";
        std::error_code existsEc2;
        if (std::filesystem::exists(name, existsEc2)) {
          LOG_TOPIC("fc54e", DEBUG, Logger::CONFIG)
              << "found config file '" << name << "'";
          filename = name;
          break;
        } else if (existsEc2) {
          LOG_TOPIC("fc54f", ERR, Logger::CONFIG)
              << "error checking config file '" << name
              << "': " << existsEc2.message();
        } else {
          LOG_TOPIC("fc550", DEBUG, Logger::CONFIG)
              << "config file '" << name << "' does not exist";
        }
      }
    }

    if (filename.empty()) {
      LOG_TOPIC("f4964", DEBUG, Logger::CONFIG)
          << "cannot find any config file";
    }

    IniFileParser parser(progOpts.get());
    std::string local = filename + ".local";

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
        progOpts->failNotice(
            TRI_EXIT_CONFIG_NOT_FOUND,
            "cannot find configuration file\n\n" + locationMsg);
        FATAL_ERROR_EXIT_CODE(progOpts->processingResult().exitCodeOrFailure());
      }
    } else if (!parser.parse(filename, true)) {
      FATAL_ERROR_EXIT_CODE(progOpts->processingResult().exitCodeOrFailure());
    }
  }

  if (configOpts.checkConfiguration) {
    exit(EXIT_SUCCESS);
  }
}

}  // namespace arangodb
