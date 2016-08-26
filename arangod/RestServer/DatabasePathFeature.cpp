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
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

DatabasePathFeature::DatabasePathFeature(ApplicationServer* server)
    : ApplicationFeature(server, "DatabasePath") {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("FileDescriptors");
  startsAfter("Language");
  startsAfter("Logger");
  startsAfter("PageSize");
  startsAfter("Random");
  startsAfter("Temp");
  startsAfter("WorkMonitor");
  startsAfter("Statistics");
}

void DatabasePathFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");
  
  options->addOption("--database.directory", "path to the database directory",
                     new StringParameter(&_directory));

}

void DatabasePathFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;

  if (1 == positionals.size()) {
    _directory = positionals[0];
  } else if (1 < positionals.size()) {
    LOG(FATAL) << "expected at most one database directory, got '"
               << StringUtils::join(positionals, ",") << "'";
    FATAL_ERROR_EXIT();
  }

  if (_directory.empty()) {
    LOG(FATAL) << "no database path has been supplied, giving up, please use "
                  "the '--database.directory' option";
    FATAL_ERROR_EXIT();
  }

  // strip trailing separators
  _directory = basics::StringUtils::rTrim(_directory, TRI_DIR_SEPARATOR_STR);
  
  auto ctx = ArangoGlobalContext::CONTEXT;
    
  if (ctx == nullptr) {
    LOG(ERR) << "failed to get global context.";
    FATAL_ERROR_EXIT();
  }

  ctx->getCheckPath(_directory, "database.directory", false);
}

void DatabasePathFeature::start() {
  // create base directory if it does not exist 
  if (!basics::FileUtils::isDirectory(_directory)) {
    std::string systemErrorStr;
    long errorNo;

    int res = TRI_CreateRecursiveDirectory(_directory.c_str(), errorNo,
                                           systemErrorStr);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG(INFO) << "created database directory '" << _directory << "'.";
    } else {
      LOG(FATAL) << "unable to create database directory '" << _directory << "': " << systemErrorStr;
      FATAL_ERROR_EXIT();
    }
  }
}

std::string DatabasePathFeature::subdirectoryName(std::string const& subDirectory) const {
  TRI_ASSERT(!_directory.empty());
  return basics::FileUtils::buildFilename(_directory, subDirectory);
}

