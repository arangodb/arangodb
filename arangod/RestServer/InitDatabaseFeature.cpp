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

#include "InitDatabaseFeature.h"

#include <iostream>

#include "Basics/FileUtils.h"
#include "Basics/terminal-utils.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

InitDatabaseFeature::InitDatabaseFeature(ApplicationServer* server,
    std::vector<std::string> const& nonServerFeatures)
  : ApplicationFeature(server, "InitDatabase"),
    _nonServerFeatures(nonServerFeatures) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
  startsAfter("DatabasePath");
}

void InitDatabaseFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");

  options->addHiddenOption("--database.init-database",
                           "initializes an empty database",
                           new BooleanParameter(&_initDatabase));

  options->addHiddenOption("--database.restore-admin",
                           "resets the admin users and sets a new password",
                           new BooleanParameter(&_restoreAdmin));

  options->addHiddenOption("--database.password",
                           "initial password of root user",
                           new StringParameter(&_password));
}

void InitDatabaseFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  ProgramOptions::ProcessingResult const& result = options->processingResult();
  _seenPassword = result.touched("database.password");

  if (_initDatabase || _restoreAdmin) {
    ApplicationServer::forceDisableFeatures(_nonServerFeatures);
    ServerState::instance()->setRole(ServerState::ROLE_SINGLE);
  }
}

void InitDatabaseFeature::prepare() {
  if (!_seenPassword) {
    std::string env = "ARANGODB_DEFAULT_ROOT_PASSWORD";
    char const* password = getenv(env.c_str());

    if (password != nullptr) {
      env += "=";
      putenv(const_cast<char*>(env.c_str()));
      _password = password;
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
          readPassword("Please enter password for root user");

      if (!password1.empty()) {
        std::string password2 = readPassword("Repeat password");

        if (password1 == password2) {
          _password = password1;
          break;
        }

        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "passwords do not match, please repeat";
      } else {
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "initialization aborted by user";
        FATAL_ERROR_EXIT();
      }
    }
  }
}

std::string InitDatabaseFeature::readPassword(std::string const& message) {
  std::string password;

  arangodb::Logger::flush();
  std::cout << message << ": " << std::flush;

#ifdef TRI_HAVE_TERMIOS_H
  TRI_SetStdinVisibility(false);
  std::getline(std::cin, password);

  TRI_SetStdinVisibility(true);
#else
  std::getline(std::cin, password);
#endif

  std::cout << std::endl;

  return password;
}

void InitDatabaseFeature::checkEmptyDatabase() {
  auto database = ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  std::string path = database->directory();
  std::string serverFile = database->subdirectoryName("SERVER");

  bool empty = false;
  std::string message;
  int code = 2;

  if (FileUtils::exists(path)) {
    if (!FileUtils::isDirectory(path)) {
      message = "database path '" + path + "' is not a directory";
      code = EXIT_FAILURE;
      goto doexit;
    }

    if (FileUtils::exists(serverFile)) {
      if (FileUtils::isDirectory(serverFile)) {
        message =
            "database SERVER '" + serverFile + "' is not a file";
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
    goto doexit;
  }

  return;

doexit:
  LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << message;

  auto logger = ApplicationServer::getFeature<LoggerFeature>("Logger");
  logger->unprepare();

  TRI_EXIT_FUNCTION(code, nullptr);
  exit(code);
}
