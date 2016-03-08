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

#include "ApplicationFeatures/DatabaseFeature.h"

#include "Logger/Logger.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "ProgramOptions2/Section.h"

using namespace arangodb;
using namespace arangodb::options;

DatabaseFeature::DatabaseFeature(
    application_features::ApplicationServer* server,
    uint64_t maximalJournalSize)
    : ApplicationFeature(server, "DatabaseFeature"),
      _directory(""),
      _maximalJournalSize(maximalJournalSize),
      _queryTracking(true),
      _queryCacheMode("off"),
      _queryCacheEntries(128) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("LoggerFeature");
}

void DatabaseFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection(Section("database", "Configure the database",
                              "database options", false, false));

  options->addOption("--database.directory", "path to the database directory",
                     new StringParameter(&_directory));

  options->addOption("--database.maximal-journal-size",
                     "default maximal journal size, can be overwritten when "
                     "creating a collection",
                     new UInt64Parameter(&_maximalJournalSize));

  options->addSection(
      Section("query", "Configure queries", "query options", false, false));

  options->addOption("--query.tracking", "wether to track queries",
                     new BooleanParameter(&_queryTracking));

  options->addOption("--query.cache-mode",
                     "mode for the AQL query cache (on, off, demand)",
                     new StringParameter(&_queryCacheMode));

  options->addOption("--query.cache-entries",
                     "maximum number of results in query cache per database",
                     new UInt64Parameter(&_queryCacheEntries));
}
