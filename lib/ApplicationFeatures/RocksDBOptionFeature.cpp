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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBOptionFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

RocksDBOptionFeature::RocksDBOptionFeature(
  application_features::ApplicationServer* server)
  : application_features::ApplicationFeature(server, "RocksDBOption"),
    _writeBufferSize(0),
    _maxWriteBufferNumber(2),
    _delayedWriteRate(2 * 1024 * 1024),
    _minWriteBufferNumberToMerge(1),
    _numLevels(4),
    _maxBytesForLevelBase(256 * 1024 * 1024),
    _maxBytesForLevelMultiplier(10),
    _baseBackgroundCompactions(1),
    _maxBackgroundCompactions(1),
    _maxLogFileSize(0),
    _keepLogFileNum(1000),
    _logFileTimeToRoll(0),
    _compactionReadaheadSize(0),
    _verifyChecksumsInCompaction(true),
    _optimizeFiltersForHits(true)
{
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("DatabasePath");
}

void RocksDBOptionFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("rocksdb", "Configure the RocksDB engine");
  
  options->addOption(
      "--rocksdb.write-buffer-size",
      "amount of data to build up in memory before converting to a sorted on-disk file (0 = disabled)",
      new UInt64Parameter(&_writeBufferSize));

  options->addOption(
      "--rocksdb.max-write-buffer-number",
      "maximum number of write buffers that built up in memory",
      new UInt64Parameter(&_maxWriteBufferNumber));

  options->addHiddenOption(
      "--rocksdb.delayed_write_rate",
      "limited write rate to DB (in bytes per second) if we are writing to the last "
      "mem table allowed and we allow more than 3 mem tables",
      new UInt64Parameter(&_delayedWriteRate));

  options->addOption(
      "--rocksdb.min-write-buffer-number-to-merge",
      "minimum number of write buffers that will be merged together before writing "
      "to storage",
      new UInt64Parameter(&_minWriteBufferNumberToMerge));

  options->addOption(
      "--rocksdb.num-levels",
      "number of levels for the database",
      new UInt64Parameter(&_numLevels));

  options->addHiddenOption(
      "--rocksdb.max-bytes-for-level-base",
      "control maximum total data size for a level",
      new UInt64Parameter(&_maxBytesForLevelBase));

  options->addOption(
      "--rocksdb.max-bytes-for-level-multiplier",
      "control maximum total data size for a level",
      new UInt64Parameter(&_maxBytesForLevelMultiplier));

  options->addOption(
      "--rocksdb.verify-checksums-in-compation",
      "if true, compaction will verify checksum on every read that happens "
      "as part of compaction",
      new BooleanParameter(&_verifyChecksumsInCompaction));

  options->addOption(
      "--rocksdb.optimize-filters-for-hits",
      "this flag specifies that the implementation should optimize the filters "
      "mainly for cases where keys are found rather than also optimize for keys "
      "missed. This would be used in cases where the application knows that "
      "there are very few misses or the performance in the case of misses is not "
      "important",
      new BooleanParameter(&_optimizeFiltersForHits));

  options->addOption(
      "--rocksdb.base-background-compactions",
      "suggested number of concurrent background compaction jobs",
      new UInt64Parameter(&_baseBackgroundCompactions));

  options->addOption(
      "--rocksdb.max-background-compactions",
      "maximum number of concurrent background compaction jobs",
      new UInt64Parameter(&_maxBackgroundCompactions));

  options->addOption(
      "--rocksdb.max-log-file-size",
      "specify the maximal size of the info log file",
      new UInt64Parameter(&_maxLogFileSize));

  options->addOption(
      "--rocksdb.keep-log-file-num",
      "maximal info log files to be kept",
      new UInt64Parameter(&_keepLogFileNum));

  options->addOption(
      "--rocksdb.log-file-time-to-roll",
      "time for the info log file to roll (in seconds). "
      "If specified with non-zero value, log file will be rolled "
      "if it has been active longer than `log_file_time_to_roll`",
      new UInt64Parameter(&_logFileTimeToRoll));

  options->addOption(
      "--rocksdb.compaction-read-ahead-size",
      "if non-zero, we perform bigger reads when doing compaction. If you're "
      "running RocksDB on spinning disks, you should set this to at least 2MB. "
      "that way RocksDB's compaction is doing sequential instead of random reads.",
      new UInt64Parameter(&_compactionReadaheadSize));
}

void RocksDBOptionFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
    if (_writeBufferSize > 0 && _writeBufferSize < 1024 * 1024) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for '--rocksdb.write-buffer-size'";
      FATAL_ERROR_EXIT();
    }
    if (_maxBytesForLevelMultiplier == 0) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for '--rocksdb.max-bytes-for-level-multiplier'";
      FATAL_ERROR_EXIT();
    }
    if (_numLevels < 1 || _numLevels > 20) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for '--rocksdb.num-levels'";
      FATAL_ERROR_EXIT();
    }
    if (_baseBackgroundCompactions < 1 || _baseBackgroundCompactions > 64) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "invalid value for '--rocksdb.base-background-compactions'";
      FATAL_ERROR_EXIT();
    }
    if (_maxBackgroundCompactions < _baseBackgroundCompactions) {
      _maxBackgroundCompactions = _baseBackgroundCompactions;
    }
}

