////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesCompactionFeature.h"

#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

MMFilesCompactionFeature* MMFilesCompactionFeature::COMPACTOR = nullptr;

MMFilesCompactionFeature::MMFilesCompactionFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "MMFilesCompaction"),
      _compactionSleepTime(1.0),
      _compactionCollectionInterval(10.0),
      _maxFiles(3),
      _maxSizeFactor(3),
      _smallDatafileSize(128 * 1024),
      _maxResultFilesize(128 * 1024 * 1024),
      _deadNumberThreshold(16384),
      _deadSizeThreshold(128 * 1024),
      _deadShare(0.1) {
  setOptional(true);
  onlyEnabledWith("MMFilesEngine");

  startsAfter("BasicsPhase");

  MMFilesCompactionFeature::COMPACTOR = this;
}

void MMFilesCompactionFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("compaction", "Configure the MMFiles compactor thread");

  options->addOption("--compaction.db-sleep-time",
                     "sleep interval between two compaction runs (in s)",
                     new DoubleParameter(&_compactionSleepTime));

  options->addOption("--compaction.min-interval",
                     "minimum sleep time between two compaction runs (in s)",
                     new DoubleParameter(&_compactionCollectionInterval));

  options->addOption("--compaction.min-small-data-file-size",
                     "minimal filesize threshhold original data files have to "
                     "be below for a compaction",
                     new UInt64Parameter(&_smallDatafileSize));

  options->addOption("--compaction.dead-documents-threshold",
                     "minimum unused count of documents in a datafile",
                     new UInt64Parameter(&_deadNumberThreshold));

  options->addOption(
      "--compaction.dead-size-threshold",
      "how many bytes of the source data file are allowed to be unused at most",
      new UInt64Parameter(&_deadSizeThreshold));

  options->addOption(
      "--compaction.dead-size-percent-threshold",
      "how many percent of the source datafile should be unused at least",
      new DoubleParameter(&_deadShare));

  options->addOption("--compaction.max-files",
                     "Maximum number of files to merge to one file",
                     new UInt64Parameter(&_maxFiles));

  options->addOption(
      "--compaction.max-result-file-size",
      "how large may the compaction result file become (in bytes)",
      new UInt64Parameter(&_maxResultFilesize));

  options->addOption("--compaction.max-file-size-factor",
                     "how large the resulting file may be in comparison to the "
                     "collections '--database.maximal-journal-size' setting",
                     new UInt64Parameter(&_maxSizeFactor));
}

void MMFilesCompactionFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  if (_deadNumberThreshold < 1024) {
    LOG_TOPIC("0ed00", WARN, Logger::COMPACTOR)
        << "compaction.dead-documents-threshold should be at least 1024.";
    _deadNumberThreshold = 1024;
  }

  if (_deadSizeThreshold < 10240) {
    LOG_TOPIC("018bf", WARN, Logger::COMPACTOR)
        << "compaction.dead-size-threshold should be at least 10k.";
    _deadSizeThreshold = 10240;
  }

  if (_deadShare < 0.001) {
    LOG_TOPIC("624da", WARN, Logger::COMPACTOR)
        << "compaction.dead-size-percent-threshold should be at least 0.001%.";
    _deadShare = 0.01;
  }

  if (_maxResultFilesize < TRI_JOURNAL_MINIMAL_SIZE) {
    LOG_TOPIC("a0f60", WARN, Logger::COMPACTOR)
        << "compaction.max-result-file-size should be at least: "
        << TRI_JOURNAL_MINIMAL_SIZE;
    _maxResultFilesize = TRI_JOURNAL_MINIMAL_SIZE;
  }

  if (_maxSizeFactor < 1) {
    LOG_TOPIC("80167", WARN, Logger::COMPACTOR)
        << "compaction.max-file-size-factor should be at least: 1";
    _maxSizeFactor = 1;
  }
}
