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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_PHYSICAL_COLLECTION_H
#define ARANGOD_VOCBASE_PHYSICAL_COLLECTION_H 1

#include "Basics/Common.h"
#include "VocBase/DatafileStatisticsContainer.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

struct TRI_datafile_t;
struct TRI_df_marker_t;
struct TRI_doc_mptr_t;

namespace arangodb {
class Ditches;
class LogicalCollection;
class Transaction;

class PhysicalCollection {
 protected:
  PhysicalCollection(LogicalCollection* collection) : _logicalCollection(collection) {}

 public:
  virtual ~PhysicalCollection() = default;
  
  virtual Ditches* ditches() const = 0;

  virtual TRI_voc_rid_t revision() const = 0;
  
  // Used for Transaction rollback
  virtual void setRevision(TRI_voc_rid_t revision, bool force) = 0;
  
  virtual int64_t initialCount() const = 0;

  virtual void updateCount(int64_t) = 0;

  virtual void figures(std::shared_ptr<arangodb::velocypack::Builder>&) = 0;
  
  virtual int close() = 0;
  
  /// @brief rotate the active journal - will do nothing if there is no journal
  virtual int rotateActiveJournal() = 0;
  
  virtual int applyForTickRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                                std::function<bool(TRI_voc_tick_t foundTick, TRI_df_marker_t const* marker)> const& callback) = 0;

  /// @brief increase dead stats for a datafile, if it exists
  virtual void increaseDeadStats(TRI_voc_fid_t fid, int64_t number, int64_t size) = 0;
  
  /// @brief increase dead stats for a datafile, if it exists
  virtual void updateStats(TRI_voc_fid_t fid, DatafileStatisticsContainer const& values) = 0;
  
  /// @brief report extra memory used by indexes etc.
  virtual size_t memory() const = 0;
    
  /// @brief order a new master pointer
  virtual TRI_doc_mptr_t* requestMasterpointer() = 0; 
  
  /// @brief release an existing master pointer
  virtual void releaseMasterpointer(TRI_doc_mptr_t* mptr) = 0;

  /// @brief disallow compaction of the collection 
  /// after this call it is guaranteed that no compaction will be started until allowCompaction() is called
  virtual void preventCompaction() = 0;

  /// @brief try disallowing compaction of the collection 
  /// returns true if compaction is disallowed, and false if not
  virtual bool tryPreventCompaction() = 0;

  /// @brief re-allow compaction of the collection 
  virtual void allowCompaction() = 0;
  
  /// @brief exclusively lock the collection for compaction
  virtual void lockForCompaction() = 0;
  
  /// @brief try to exclusively lock the collection for compaction
  /// after this call it is guaranteed that no compaction will be started until allowCompaction() is called
  virtual bool tryLockForCompaction() = 0;

  /// @brief signal that compaction is finished
  virtual void finishCompaction() = 0;

  /// @brief iterate all markers of a collection on load
  virtual int iterateMarkersOnLoad(arangodb::Transaction* trx) = 0;
  
 protected:
  LogicalCollection* _logicalCollection;
};

} // namespace arangodb

#endif
