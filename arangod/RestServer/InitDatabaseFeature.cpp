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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "InitDatabaseFeature.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <locale.h>
#include <string.h>
#include <tchar.h>
#include <unicode/locid.h>
#include <iomanip>
#endif

#include <chrono>
#include <iostream>
#include <thread>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/EnvironmentFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "Basics/files.h"
#include "Basics/terminal-utils.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

InitDatabaseFeature::InitDatabaseFeature(application_features::ApplicationServer& server,
                                         std::vector<std::type_index> const& nonServerFeatures)
    : ApplicationFeature(server, "InitDatabase"),
      _nonServerFeatures(nonServerFeatures) {
  setOptional(false);
  startsAfter<BasicFeaturePhaseServer>();
}

void InitDatabaseFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");

  options->addOption("--database.init-database", "initializes an empty database",
                     new BooleanParameter(&_initDatabase),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden,
                                                  arangodb::options::Flags::Command));

  options->addOption("--database.restore-admin",
                     "resets the admin users and sets a new password",
                     new BooleanParameter(&_restoreAdmin),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden,
                                                  arangodb::options::Flags::Command));

  options->addOption("--database.password", "initial password of root user",
                     new StringParameter(&_password),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
}

void InitDatabaseFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  ProgramOptions::ProcessingResult const& result = options->processingResult();
  _seenPassword = result.touched("database.password");

  if (_initDatabase || _restoreAdmin) {
    server().forceDisableFeatures(_nonServerFeatures);
    ServerState::instance()->setRole(ServerState::ROLE_SINGLE);

    // we can turn off all warnings about environment here, because they
    // wil show up on a regular start later anyway
    server().disableFeatures(
        std::vector<std::type_index>{std::type_index(typeid(EnvironmentFeature))});
  }
}

void InitDatabaseFeature::prepare() {
  if (!_seenPassword) {
    if (TRI_GETENV("ARANGODB_DEFAULT_ROOT_PASSWORD", _password)) {
      _seenPassword = true;
    }
  }

  if (!_initDatabase && !_restoreAdmin) {
    return;
  }

  if (_initDatabase) {
    checkEmptyDatabase();
  }

  if (!_seenPassword) {
    while (true) {
      std::string password1 =
          readPassword("Please enter a new password for the ArangoDB root user");

      if (!password1.empty()) {
        std::string password2 = readPassword("Repeat password");

        if (password1 == password2) {
          _password = password1;
          break;
        }
        LOG_TOPIC("2a01c", ERR, arangodb::Logger::FIXME)
            << "passwords do not match, please repeat";
      } else {
        LOG_TOPIC("ba459", FATAL, arangodb::Logger::FIXME)
            << "initialization aborted by user";
        FATAL_ERROR_EXIT();
      }
    }
  }
}

std::string InitDatabaseFeature::readPassword(std::string const& message) {
  std::string password;

  arangodb::Logger::flush();
  // Wait for the logger thread to flush eventually existing output.
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cerr << std::flush;
  std::cout << message << ": " << std::flush;
#ifdef _WIN32
  TRI_SetStdinVisibility(false);
  TRI_DEFER(TRI_SetStdinVisibility(true));
  std::wstring wpassword;
  _setmode(_fileno(stdin), _O_U16TEXT);
  std::getline(std::wcin, wpassword);
  icu::UnicodeString pw(wpassword.c_str(), static_cast<int32_t>(wpassword.length()));
  pw.toUTF8String<std::string>(password);
#else
#ifdef TRI_HAVE_TERMIOS_H
  TRI_SetStdinVisibility(false);
  std::getline(std::cin, password);

  TRI_SetStdinVisibility(true);
#else
  std::getline(std::cin, password);
#endif
#endif
  std::cout << std::endl;

  return password;
}

void InitDatabaseFeature::checkEmptyDatabase() {
  auto& database = server().getFeature<DatabasePathFeature>();
  std::string path = database.directory();
  std::string serverFile = database.subdirectoryName("SERVER");

  bool empty = false;
  std::string message;
  int code = TRI_EXIT_CODE_RESOLVING_FAILED;

  if (FileUtils::exists(path)) {
    if (!FileUtils::isDirectory(path)) {
      message = "database path '" + path + "' is not a directory";
      code = EXIT_FAILURE;
      goto doexit;
    }

    if (FileUtils::exists(serverFile)) {
      if (FileUtils::isDirectory(serverFile)) {
        message = "database SERVER '" + serverFile + "' is not a file";
        code = EXIT_FAILURE;
        goto doexit;
      }
    } else {
      empty = true;
    }
  } else {
    empty = true;
  }

  if (!empty) {
    message = "database already initialized, refusing to initialize it again";
    code = TRI_EXIT_DB_NOT_EMPTY;
    goto doexit;
  }

  return;

doexit:
  LOG_TOPIC("a38e6", FATAL, arangodb::Logger::FIXME) << message;

  auto& logger = server().getFeature<LoggerFeature>();
  logger.unprepare();

  TRI_EXIT_FUNCTION(code, nullptr);
  exit(code);
}

}  // namespace arangodb
