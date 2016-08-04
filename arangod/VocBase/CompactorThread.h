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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_COMPACTOR_THREAD_H
#define ARANGOD_VOC_BASE_COMPACTOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "VocBase/voc-types.h"

struct TRI_collection_t;
struct TRI_datafile_t;
struct TRI_vocbase_t;

namespace arangodb {

class CompactorThread : public Thread {
 public:
  /// @brief compaction instruction for a single datafile
  struct compaction_info_t {
    TRI_datafile_t* _datafile;
    bool _keepDeletions;
  };

 public:
  explicit CompactorThread(TRI_vocbase_t* vocbase);
  ~CompactorThread();

  void signal();

 protected:
  void run() override;

 private:
  /// @brief compact the specified datafiles
  void compactDatafiles(TRI_collection_t*, std::vector<compaction_info_t> const&);

  /// @brief checks all datafiles of a collection
  bool compactCollection(TRI_collection_t*);

  /// @brief wait time between compaction runs when idle
  static constexpr unsigned compactionSleepTime() { return 1000 * 1000; }

  /// @brief compaction interval in seconds
  static constexpr double compactionCollectionInterval() { return 10.0; }
  
  /// @brief maximum number of files to compact and concat
  static constexpr unsigned maxFiles() { return 3; }

  /// @brief maximum multiple of journal filesize of a compacted file
  /// a value of 3 means that the maximum filesize of the compacted file is
  /// 3 x (collection->journalSize)
  static constexpr unsigned maxSizeFactor() { return 3; }

  static constexpr unsigned smallDatafileSize() { return 128 * 1024; }
  
  /// @brief maximum filesize of resulting compacted file
  static constexpr uint64_t maxResultFilesize() { return 128 * 1024 * 1024; }

  /// @brief minimum number of deletion marker in file from which on we will
  /// compact it if nothing else qualifies file for compaction
  static constexpr int64_t deadNumberThreshold() { return 16384; }

  /// @brief minimum size of dead data (in bytes) in a datafile that will make
  /// the datafile eligible for compaction at all.
  /// Any datafile with less dead data than the threshold will not become a
  /// candidate for compaction.
  static constexpr int64_t deadSizeThreshold() { return 128 * 1024; }

  /// @brief percentage of dead documents in a datafile that will trigger the
  /// compaction
  /// for example, if the collection contains 800 bytes of alive and 400 bytes of
  /// dead documents, the share of the dead documents is 400 / (400 + 800) = 33 %.
  /// if this value if higher than the threshold, the datafile will be compacted
  static constexpr double deadShare() { return 0.1; }


 private:
  TRI_vocbase_t* _vocbase;

  arangodb::basics::ConditionVariable _condition;
};

}

/// @brief remove data of expired compaction blockers
bool TRI_CleanupCompactorVocBase(TRI_vocbase_t*);

/// @brief insert a compaction blocker
int TRI_InsertBlockerCompactorVocBase(TRI_vocbase_t*, double, TRI_voc_tick_t*);

/// @brief touch an existing compaction blocker
int TRI_TouchBlockerCompactorVocBase(TRI_vocbase_t*, TRI_voc_tick_t, double);

/// @brief remove an existing compaction blocker
int TRI_RemoveBlockerCompactorVocBase(TRI_vocbase_t*, TRI_voc_tick_t);

/// @brief compactor event loop
void TRI_CompactorVocBase(void*);

#endif
