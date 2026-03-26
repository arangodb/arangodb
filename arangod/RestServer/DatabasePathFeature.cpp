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

#include "DatabasePathFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Basics/operating-system.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

DatabasePathFeature::DatabasePathFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature{server, *this} {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();

#ifdef TRI_HAVE_GETRLIMIT
  startsAfter<FileDescriptorsFeature>();
#endif
  startsAfter<LanguageFeature>();
  startsAfter<TempFeature>();
}

void DatabasePathFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options
      ->addOption("--database.directory", "The path to the database directory.",
                  new StringParameter(&_options.directory))
      .setLongDescription(R"(This defines the location where all data of a
server is stored.

Make sure the directory is writable by the arangod process. You should further
not use a database directory which is provided by a network filesystem such as
NFS. The reason is that networked filesystems might cause inconsistencies when
there are multiple parallel readers or writers or they lack features required by
arangod, e.g. `flock()`.)");

  options->addOption(
      "--database.required-directory-state",
      "The required state of the database directory at startup "
      "(non-existing: the database directory must not exist, existing: the"
      "database directory must exist, empty: the database directory must exist "
      "but be empty, populated: the database directory must exist and contain "
      "specific files already, any: any state is allowed)",
      new DiscreteValuesParameter<StringParameter>(
          &_options.requiredDirectoryState,
          std::unordered_set<std::string>{"any", "non-existing", "existing",
                                          "empty", "populated"}));
}

void DatabasePathFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;

  if (1 == positionals.size()) {
    _options.directory = positionals[0];
  } else if (1 < positionals.size()) {
    LOG_TOPIC("aeb40", FATAL, arangodb::Logger::FIXME)
        << "expected at most one database directory, got '"
        << StringUtils::join(positionals, ",") << "'";
    FATAL_ERROR_EXIT();
  }

  if (_options.directory.empty()) {
    LOG_TOPIC("9aba1", FATAL, arangodb::Logger::FIXME)
        << "no database path has been supplied, giving up, please use "
           "the '--database.directory' option";
    FATAL_ERROR_EXIT();
  }

  // strip trailing separators
  _options.directory =
      basics::StringUtils::rTrim(_options.directory, TRI_DIR_SEPARATOR_STR);

  auto ctx = ArangoGlobalContext::CONTEXT;

  if (ctx == nullptr) {
    LOG_TOPIC("19066", FATAL, arangodb::Logger::FIXME)
        << "failed to get global context.";
    FATAL_ERROR_EXIT();
  }

  ctx->normalizePath(_options.directory, "database.directory", false);
}

void DatabasePathFeature::prepare() {
  // check if temporary directory and database directory are identical
  {
    std::string directoryCopy = _options.directory;
    basics::FileUtils::makePathAbsolute(directoryCopy);

    if (server().hasFeature<TempFeature>()) {
      auto& tf = server().getFeature<TempFeature>();
      // the feature is not present in unit tests, so make the execution depend
      // on whether the feature is available
      std::string tempPathCopy = tf.path();
      basics::FileUtils::makePathAbsolute(tempPathCopy);
      tempPathCopy =
          basics::StringUtils::rTrim(tempPathCopy, TRI_DIR_SEPARATOR_STR);

      if (directoryCopy == tempPathCopy) {
        LOG_TOPIC("fd70b", FATAL, arangodb::Logger::FIXME)
            << "database directory '" << directoryCopy
            << "' is identical to the temporary directory. "
            << "This can cause follow-up problems, including data loss. Please "
               "review your setup!";
        FATAL_ERROR_EXIT();
      }
    }
  }

  if (_options.requiredDirectoryState == "any") {
    // database directory can have any state. this is the default
    return;
  }

  if (_options.requiredDirectoryState == "non-existing") {
    if (basics::FileUtils::isDirectory(_options.directory)) {
      LOG_TOPIC("25452", FATAL, arangodb::Logger::STARTUP)
          << "database directory '" << _options.directory
          << "' already exists, but option "
             "'--database.required-directory-state' was set to 'non-existing'";
      FATAL_ERROR_EXIT();
    }
    return;
  }

  // existing, empty, populated when we get here
  if (!basics::FileUtils::isDirectory(_options.directory)) {
    LOG_TOPIC("3b1df", FATAL, arangodb::Logger::STARTUP)
        << "database directory '" << _options.directory
        << "' does not exist, but option '--database.required-directory-state' "
           "was set to '"
        << _options.requiredDirectoryState << "'";
    FATAL_ERROR_EXIT();
  }

  if (_options.requiredDirectoryState == "existing") {
    // directory exists. all good
    return;
  }

  std::vector<std::string> files;
  for (auto const& it : basics::FileUtils::listFiles(_options.directory)) {
    if (it.empty() || basics::FileUtils::isDirectory(it)) {
      continue;
    }

    // we are interested in just the filenames
    files.emplace_back(TRI_Basename(it.c_str()));
  }

  if (_options.requiredDirectoryState == "empty" && !files.empty()) {
    LOG_TOPIC("508c5", FATAL, arangodb::Logger::STARTUP)
        << "database directory '" << _options.directory
        << "' is not empty, but option '--database.required-directory-state' "
           "was set to '"
        << _options.requiredDirectoryState << "'";
    FATAL_ERROR_EXIT();
  }

  if (_options.requiredDirectoryState == "populated" &&
      (std::find(files.begin(), files.end(), "ENGINE") == files.end() ||
       std::find(files.begin(), files.end(), "SERVER") == files.end())) {
    LOG_TOPIC("962c7", FATAL, arangodb::Logger::STARTUP)
        << "database directory '" << _options.directory
        << "' is not properly populated, but option "
           "'--database.required-directory-state' was set to '"
        << _options.requiredDirectoryState << "'";
    FATAL_ERROR_EXIT();
  }

  // all good here
}

void DatabasePathFeature::start() {
  // create base directory if it does not exist
  if (!basics::FileUtils::isDirectory(_options.directory)) {
    std::string systemErrorStr;
    long errorNo;

    auto const res = TRI_CreateRecursiveDirectory(_options.directory.c_str(),
                                                  errorNo, systemErrorStr);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("24783", INFO, arangodb::Logger::FIXME)
          << "created database directory '" << _options.directory << "'";
    } else {
      LOG_TOPIC("1e987", FATAL, arangodb::Logger::FIXME)
          << "unable to create database directory '" << _options.directory
          << "': " << systemErrorStr;
      FATAL_ERROR_EXIT();
    }
  }
}

std::string DatabasePathFeature::subdirectoryName(
    std::string const& subDirectory) const {
  TRI_ASSERT(!_options.directory.empty());
  return basics::FileUtils::buildFilename(_options.directory, subDirectory);
}

}  // namespace arangodb
