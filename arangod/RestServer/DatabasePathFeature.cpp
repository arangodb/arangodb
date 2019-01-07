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

#include "DatabasePathFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

DatabasePathFeature::DatabasePathFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, DatabasePathFeature::name()),
      _requiredDirectoryState("any") {
  setOptional(false);
  startsAfter("GreetingsPhase");

  startsAfter("FileDescriptors");
  startsAfter("Language");
  startsAfter("PageSize");
  startsAfter("Temp");
}

void DatabasePathFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");

  options->addOption("--database.directory", "path to the database directory",
                     new StringParameter(&_directory));

  options->addOption(
      "--database.required-directory-state",
      "required state of database directory at startup "
      "(non-existing: database directory must not exist, existing: database "
      "directory must exist, "
      "empty: database directory must exist but be empty, "
      "populated: database directory must exist and contain specific files "
      "already, "
      "any: any state allowed)",
      new DiscreteValuesParameter<StringParameter>(
          &_requiredDirectoryState,
          std::unordered_set<std::string>{"any", "non-existing", "existing",
                                          "empty", "populated"}));
}

void DatabasePathFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;

  if (1 == positionals.size()) {
    _directory = positionals[0];
  } else if (1 < positionals.size()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "expected at most one database directory, got '"
        << StringUtils::join(positionals, ",") << "'";
    FATAL_ERROR_EXIT();
  }

  if (_directory.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "no database path has been supplied, giving up, please use "
           "the '--database.directory' option";
    FATAL_ERROR_EXIT();
  }

  // strip trailing separators
  _directory = basics::StringUtils::rTrim(_directory, TRI_DIR_SEPARATOR_STR);

  auto ctx = ArangoGlobalContext::CONTEXT;

  if (ctx == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "failed to get global context.";
    FATAL_ERROR_EXIT();
  }

  ctx->normalizePath(_directory, "database.directory", false);
}

void DatabasePathFeature::prepare() {
  if (_requiredDirectoryState == "any") {
    // database directory can have any state. this is the default
    return;
  }

  if (_requiredDirectoryState == "non-existing") {
    if (basics::FileUtils::isDirectory(_directory)) {
      LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
          << "database directory '" << _directory
          << "' already exists, but option "
             "'--database.required-directory-state' was set to 'non-existing'";
      FATAL_ERROR_EXIT();
    }
    return;
  }

  // existing, empty, populated when we get here
  if (!basics::FileUtils::isDirectory(_directory)) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "database directory '" << _directory
        << "' does not exist, but option '--database.required-directory-state' "
           "was set to '"
        << _requiredDirectoryState << "'";
    FATAL_ERROR_EXIT();
  }

  if (_requiredDirectoryState == "existing") {
    // directory exists. all good
    return;
  }

  std::vector<std::string> files;
  for (auto const& it : basics::FileUtils::listFiles(_directory)) {
    if (it.empty() || basics::FileUtils::isDirectory(it)) {
      continue;
    }

    // we are interested in just the filenames
    files.emplace_back(TRI_Basename(it.c_str()));
  }

  if (_requiredDirectoryState == "empty" && !files.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "database directory '" << _directory
        << "' is not empty, but option '--database.required-directory-state' "
           "was set to '"
        << _requiredDirectoryState << "'";
    FATAL_ERROR_EXIT();
  }

  if (_requiredDirectoryState == "populated" &&
      (std::find(files.begin(), files.end(), "ENGINE") == files.end() ||
       std::find(files.begin(), files.end(), "SERVER") == files.end())) {
    LOG_TOPIC(FATAL, arangodb::Logger::STARTUP)
        << "database directory '" << _directory
        << "' is not properly populated, but option "
           "'--database.required-directory-state' was set to '"
        << _requiredDirectoryState << "'";
    FATAL_ERROR_EXIT();
  }

  // all good here
}

void DatabasePathFeature::start() {
  // create base directory if it does not exist
  if (!basics::FileUtils::isDirectory(_directory)) {
    std::string systemErrorStr;
    long errorNo;

    int res = TRI_CreateRecursiveDirectory(_directory.c_str(), errorNo, systemErrorStr);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(INFO, arangodb::Logger::FIXME)
          << "created database directory '" << _directory << "'";
    } else {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
          << "unable to create database directory '" << _directory
          << "': " << systemErrorStr;
      FATAL_ERROR_EXIT();
    }
  }
}

std::string DatabasePathFeature::subdirectoryName(std::string const& subDirectory) const {
  TRI_ASSERT(!_directory.empty());
  return basics::FileUtils::buildFilename(_directory, subDirectory);
}

}  // namespace arangodb
