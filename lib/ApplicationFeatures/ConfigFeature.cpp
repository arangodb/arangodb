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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ConfigFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/VersionFeature.h"
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
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Translator.h"

#include <cstdlib>

#ifdef __linux__
#ifdef __GLIBC__
#include <nss.h>
#endif
#endif

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

void ConfigFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--configuration,-c",
                     "The configuration file or \"none\".",
                     new StringParameter(&_file));

  // add --config as an alias for --configuration. both point to the same
  // variable!
  options->addOption(
      "--config", "The configuration file or \"none\".",
      new StringParameter(&_file),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--define,-D",
      "Define a value for a `@key@` entry in the configuration file using the "
      "syntax `\"key=value\"`.",
      new VectorParameter<StringParameter>(&_defines),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--check-configuration", "Check the configuration and exit.",
      new BooleanParameter(&_checkConfiguration),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon,
                                          arangodb::options::Flags::Command));

  options->addOption(
      "--honor-nsswitch",
      "Allow hostname lookup configuration via /etc/nsswitch.conf if on "
      "Linux/glibc.",
      new BooleanParameter(&_honorNsswitch),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
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
    LOG_TOPIC("6cb22", DEBUG, Logger::CONFIG) << "using no config file at all";
    return;
  }

  bool fatal = true;

  if (_version) {
    fatal = !_version->printVersion();
  }

  // always prefer an explicitly given config file
  if (!_file.empty()) {
    if (!FileUtils::exists(_file)) {
      LOG_TOPIC("f21f9", FATAL, Logger::CONFIG)
          << "cannot read config file '" << _file << "'";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_CONFIG_NOT_FOUND);
    }

    auto local = _file + ".local";

    IniFileParser parser(options.get());

    if (FileUtils::exists(local) && FileUtils::isRegularFile(local)) {
      LOG_TOPIC("9b20a", DEBUG, Logger::CONFIG)
          << "loading override '" << local << "'";

      if (!parser.parse(local, true)) {
        FATAL_ERROR_EXIT_CODE(options->processingResult().exitCodeOrFailure());
      }
    }

    LOG_TOPIC("637c7", DEBUG, Logger::CONFIG)
        << "using user supplied config file '" << _file << "'";

    if (!parser.parse(_file, true)) {
      FATAL_ERROR_EXIT_CODE(options->processingResult().exitCodeOrFailure());
    }

    return;
  }

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

  auto context = ArangoGlobalContext::CONTEXT;
  std::string basename = progname;
  bool checkArangoImp = (progname == "arangoimport");

  if (!basename.ends_with(".conf")) {
    basename += ".conf";
  }

  std::vector<std::string> locations;
  locations.reserve(4);

  std::string current = FileUtils::currentDirectory().result();
  // ./etc/relative/ is always first choice, if it exists
  locations.emplace_back(FileUtils::buildFilename(current, "etc", "relative"));

  if (context != nullptr) {
    auto root = context->runRoot();
    // will resolve to ./build/etc/arangodb3/ in maintainer builds
    auto location = FileUtils::buildFilename(root, _SYSCONFDIR_);

    LOG_TOPIC("f39d1", TRACE, Logger::CONFIG)
        << "checking root location '" << root << "'";

    locations.emplace_back(location);
  }

  // ./
  locations.emplace_back(current);

  // ~/.arangodb/
  locations.emplace_back(
      FileUtils::buildFilename(FileUtils::homeDirectory(), ".arangodb"));
  locations.emplace_back(FileUtils::configDirectory(binaryPath));

  std::string filename;

  for (auto const& location : locations) {
    auto name = FileUtils::buildFilename(location, basename);
    LOG_TOPIC("393e7", TRACE, Logger::CONFIG)
        << "checking config file '" << name << "'";

    if (FileUtils::exists(name)) {
      LOG_TOPIC("e6bd8", DEBUG, Logger::CONFIG)
          << "found config file '" << name << "'";
      filename = name;
      break;
    }

    if (checkArangoImp) {
      name = FileUtils::buildFilename(location, "arangoimp.conf");
      LOG_TOPIC("b629e", TRACE, Logger::CONFIG)
          << "checking config file '" << name << "'";
      if (FileUtils::exists(name)) {
        LOG_TOPIC("fc54e", DEBUG, Logger::CONFIG)
            << "found config file '" << name << "'";
        filename = name;
        break;
      }
    }
  }

  if (filename.empty()) {
    LOG_TOPIC("f4964", DEBUG, Logger::CONFIG) << "cannot find any config file";
  }

  IniFileParser parser(options.get());
  std::string local = filename + ".local";

  LOG_TOPIC("f6420", TRACE, Logger::CONFIG)
      << "checking override '" << local << "'";

  if (FileUtils::exists(local) && FileUtils::isRegularFile(local)) {
    LOG_TOPIC("3d2d0", DEBUG, Logger::CONFIG)
        << "loading override '" << local << "'";

    if (!parser.parse(local, true)) {
      FATAL_ERROR_EXIT_CODE(options->processingResult().exitCodeOrFailure());
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
      options->failNotice(TRI_EXIT_CONFIG_NOT_FOUND,
                          "cannot find configuration file\n\n" + locationMsg);
      FATAL_ERROR_EXIT_CODE(options->processingResult().exitCodeOrFailure());
    } else {
      return;
    }
  }

  if (!parser.parse(filename, true)) {
    FATAL_ERROR_EXIT_CODE(options->processingResult().exitCodeOrFailure());
  }
}

void ConfigFeature::prepare() {
#ifdef __linux__
#ifdef __GLIBC__
  // This code deserves and explanation.
  // Our release builds use Ubuntu 24.04 with glibc 2.39.0 (at the time of this
  // writing) and build static executables. This is all nice and convenient but
  // it has one disadvantage: If host name lookups or user name lookups happen,
  // the glibc uses the configuration file /etc/nsswitch.conf to decide how to
  // do these lookups. This is a runtime configuration option of glibc.
  // Unfortunately, glibc implements some of the options via dynamically loaded
  // modules (notably mdns4_minimal via libnss_mdns4_minimal.so) and does not
  // do versioned symbols for this.
  // If this happens on a system with a different version of glibc installed
  // (like for example an older Ubuntu system or a Debian or RedHat system),
  // then glibc tries to dynamically load a module which does not fit and the
  // process crashes with a high likelihood. To prevent this, we use the
  // (undocumented) override function below. This has the consequence that the
  // host name lookup will always just use /etc/hosts and normal DNS lookup. And
  // username lookup will always just use /etc/passwd, regardless of the system
  // configuration. There is an opt-out for this in form of the configuration
  // option --honor-nsswitch. Use this only if you are running on a system
  // without glibc installed, or with glibc version 2.39.0.
  if (!_honorNsswitch) {
    __nss_configure_lookup("hosts", "files dns");
    __nss_configure_lookup("passwd", "files");
    __nss_configure_lookup("group", "files");
  }
#endif
#endif
}
}  // namespace arangodb
