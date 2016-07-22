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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "IndexPoolFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ThreadPool.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

IndexPoolFeature::IndexPoolFeature(ApplicationServer* server)
    : ApplicationFeature(server, "IndexPool"),
      _indexThreads(2) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("DatabasePath");
  startsAfter("EngineSelector");
}

void IndexPoolFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");
  
  options->addHiddenOption(
      "--database.index-threads",
      "threads to start for parallel background index creation",
      new UInt64Parameter(&_indexThreads));
}

void IndexPoolFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // some arbitrary limit
  if (_indexThreads > 128) {
    _indexThreads = 128;
  }
}

void IndexPoolFeature::start() {
  // create the index thread pool
  if (_indexThreads > 0) {
    _indexPool.reset(new ThreadPool(static_cast<size_t>(_indexThreads), "IndexBuilder"));
  }
  LOG(TRACE) << "starting " << _indexThreads << " index thread(s)";
}

void IndexPoolFeature::unprepare() {
  LOG(TRACE) << "stopping index thread(s)";
  // turn off index threads
  _indexPool.reset();
}

