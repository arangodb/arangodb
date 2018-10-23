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

#ifndef ARANGOD_MMFILES_MMFILES_COMPACTOR_THREAD_H
#define ARANGOD_MMFILES_MMFILES_COMPACTOR_THREAD_H 1

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "VocBase/voc-types.h"

struct MMFilesDatafile;
struct MMFilesMarker;
struct TRI_vocbase_t;

namespace arangodb {

struct CompactionContext;
class LogicalCollection;

namespace transaction {

class Methods;

}

class MMFilesCompactorThread final : public Thread {
 private:
  /// @brief compaction instruction for a single datafile
  struct CompactionInfo {
    MMFilesDatafile* _datafile;
    bool _keepDeletions;
  };

  /// @brief auxiliary struct used when initializing compaction
  struct CompactionInitialContext {
    transaction::Methods* _trx;
    LogicalCollection* _collection;
    int64_t _targetSize;
    TRI_voc_fid_t _fid;
    bool _keepDeletions;
    bool _failed;

    CompactionInitialContext(transaction::Methods* trx, LogicalCollection* collection)
        : _trx(trx), _collection(collection), _targetSize(0), _fid(0), _keepDeletions(false), _failed(false) {}
  };

 public:
  explicit MMFilesCompactorThread(TRI_vocbase_t& vocbase);
  ~MMFilesCompactorThread();

  void signal();

  /// @brief callback to drop a datafile
  static void DropDatafileCallback(MMFilesDatafile* datafile, LogicalCollection* collection);
  /// @brief callback to rename a datafile
  static void RenameDatafileCallback(MMFilesDatafile* datafile, MMFilesDatafile* compactor, LogicalCollection* collection);

 protected:
  void run() override;

 private:
  /// @brief calculate the target size for the compactor to be created
  CompactionInitialContext getCompactionContext(
    transaction::Methods* trx, LogicalCollection* collection,
    std::vector<CompactionInfo> const& toCompact);

  /// @brief compact the specified datafiles
  void compactDatafiles(LogicalCollection* collection, std::vector<CompactionInfo> const&);

  /// @brief checks all datafiles of a collection
  bool compactCollection(LogicalCollection* collection, bool& wasBlocked);

  int removeCompactor(LogicalCollection* collection, MMFilesDatafile* datafile);

  /// @brief remove an empty datafile
  int removeDatafile(LogicalCollection* collection, MMFilesDatafile* datafile);

  /// @brief determine the number of documents in the collection
  uint64_t getNumberOfDocuments(LogicalCollection& collection);

  /// @brief write a copy of the marker into the datafile
  int copyMarker(MMFilesDatafile* compactor, MMFilesMarker const* marker,
                 MMFilesMarker** result);

  TRI_vocbase_t& _vocbase;
  arangodb::basics::ConditionVariable _condition;
};

}

#endif
