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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_COMPACTION_FEATURE_H
#define ARANGOD_MMFILES_COMPACTION_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace options {
class ProgramOptions;
}

class MMFilesCompactionFeature : public application_features::ApplicationFeature {
 public:
  static MMFilesCompactionFeature* COMPACTOR;
 private:
  /// @brief wait time between compaction runs when idle
  double _compactionSleepTime;

  /// @brief compaction interval in seconds
  double _compactionCollectionInterval;
  
  /// @brief maximum number of files to compact and concat
  uint64_t _maxFiles;

  /// @brief maximum multiple of journal filesize of a compacted file
  /// a value of 3 means that the maximum filesize of the compacted file is
  /// 3 x (collection->journalSize)
  uint64_t _maxSizeFactor;

  uint64_t _smallDatafileSize;
  
  /// @brief maximum filesize of resulting compacted file
  uint64_t _maxResultFilesize;

  /// @brief minimum number of deletion marker in file from which on we will
  /// compact it if nothing else qualifies file for compaction
  uint64_t _deadNumberThreshold;

  /// @brief minimum size of dead data (in bytes) in a datafile that will make
  /// the datafile eligible for compaction at all.
  /// Any datafile with less dead data than the threshold will not become a
  /// candidate for compaction.
  uint64_t _deadSizeThreshold;

  /// @brief percentage of dead documents in a datafile that will trigger the
  /// compaction
  /// for example, if the collection contains 800 bytes of alive and 400 bytes of
  /// dead documents, the share of the dead documents is 400 / (400 + 800) = 33 %.
  /// if this value if higher than the threshold, the datafile will be compacted
  double _deadShare;

  MMFilesCompactionFeature(MMFilesCompactionFeature const&) = delete;
  MMFilesCompactionFeature& operator=(MMFilesCompactionFeature const&) = delete;

 public:
  explicit MMFilesCompactionFeature(
    application_features::ApplicationServer& server
  );
  ~MMFilesCompactionFeature() {}

  /// @brief wait time between compaction runs when idle
  uint64_t compactionSleepTime() const { return static_cast<uint64_t>(_compactionSleepTime) * 1000000ULL; }

  /// @brief compaction interval in seconds
  double compactionCollectionInterval() const { return _compactionCollectionInterval; }
  
  /// @brief maximum number of files to compact and concat
  size_t maxFiles() const { return static_cast<size_t>(_maxFiles); }

  /// @brief maximum multiple of journal filesize of a compacted file
  /// a value of 3 means that the maximum filesize of the compacted file is
  /// 3 x (collection->journalSize)
  uint64_t maxSizeFactor() const { return _maxSizeFactor; }

  uint64_t smallDatafileSize() const { return _smallDatafileSize; }
  
  /// @brief maximum filesize of resulting compacted file
  uint64_t maxResultFilesize() const { return _maxResultFilesize; }

  /// @brief minimum number of deletion marker in file from which on we will
  /// compact it if nothing else qualifies file for compaction
  int64_t deadNumberThreshold() const { return _deadNumberThreshold; }

  /// @brief minimum size of dead data (in bytes) in a datafile that will make
  /// the datafile eligible for compaction at all.
  /// Any datafile with less dead data than the threshold will not become a
  /// candidate for compaction.
  int64_t deadSizeThreshold() const { return _deadSizeThreshold; }

  /// @brief percentage of dead documents in a datafile that will trigger the
  /// compaction
  /// for example, if the collection contains 800 bytes of alive and 400 bytes of
  /// dead documents, the share of the dead documents is 400 / (400 + 800) = 33 %.
  /// if this value if higher than the threshold, the datafile will be compacted
  double deadShare() const { return _deadShare; }

  
 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
};

}

#endif
