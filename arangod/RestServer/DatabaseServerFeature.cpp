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

#include "DatabaseServerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/ThreadPool.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/server.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

TRI_server_t* DatabaseServerFeature::SERVER;
basics::ThreadPool* DatabaseServerFeature::INDEX_POOL;

DatabaseServerFeature::DatabaseServerFeature(ApplicationServer* server)
    : ApplicationFeature(server, "DatabaseServer"),
      _indexThreads(2),
      _server(nullptr) {
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

void DatabaseServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");
  
  options->addOption("--database.directory", "path to the database directory",
                     new StringParameter(&_directory));

  options->addHiddenOption(
      "--database.index-threads",
      "threads to start for parallel background index creation",
      new UInt64Parameter(&_indexThreads));
}

void DatabaseServerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // some arbitrary limit
  if (_indexThreads > 128) {
    _indexThreads = 128;
  }
  
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
  _directory = StringUtils::rTrim(_directory, TRI_DIR_SEPARATOR_STR);
}

void DatabaseServerFeature::prepare() {
  // create the server
  _server.reset(new TRI_server_t());
  SERVER = _server.get();
}

void DatabaseServerFeature::start() {
  // create the index thread pool
  if (_indexThreads > 0) {
    _indexPool.reset(new ThreadPool(static_cast<size_t>(_indexThreads), "IndexBuilder"));
    INDEX_POOL = _indexPool.get();
  }

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

void DatabaseServerFeature::unprepare() {
  // turn off index threads
  INDEX_POOL = nullptr;
  _indexPool.reset();

  // delete the server
  TRI_StopServer(_server.get());
  SERVER = nullptr;
  _server.reset(nullptr);

  // done
  LOG(INFO) << "ArangoDB has been shut down";
}
