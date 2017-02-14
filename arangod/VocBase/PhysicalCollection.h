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

#ifndef ARANGOD_VOCBASE_PHYSICAL_COLLECTION_H
#define ARANGOD_VOCBASE_PHYSICAL_COLLECTION_H 1

#include "Basics/Common.h"
#include "VocBase/DatafileStatisticsContainer.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

struct TRI_df_marker_t;

namespace arangodb {
namespace transaction {
class Methods;
}

class Ditches;
class LogicalCollection;
class ManagedDocumentResult;
struct OperationOptions;

class PhysicalCollection {
 protected:
  explicit PhysicalCollection(LogicalCollection* collection) : _logicalCollection(collection) {}

 public:
  virtual ~PhysicalCollection() = default;
  
  virtual Ditches* ditches() const = 0;

  virtual TRI_voc_rid_t revision() const = 0;
  
  // Used for transaction::Methods rollback
  virtual void setRevision(TRI_voc_rid_t revision, bool force) = 0;
  
  virtual int64_t initialCount() const = 0;

  virtual void updateCount(int64_t) = 0;

  virtual void figures(std::shared_ptr<arangodb::velocypack::Builder>&) = 0;
  
  virtual int close() = 0;
  
  /// @brief rotate the active journal - will do nothing if there is no journal
  virtual int rotateActiveJournal() = 0;
  
  virtual bool applyForTickRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                                 std::function<bool(TRI_voc_tick_t foundTick, TRI_df_marker_t const* marker)> const& callback) = 0;

  /// @brief increase dead stats for a datafile, if it exists
  virtual void updateStats(TRI_voc_fid_t fid, DatafileStatisticsContainer const& values) = 0;
      
  /// @brief report extra memory used by indexes etc.
  virtual size_t memory() const = 0;
    
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
  virtual int iterateMarkersOnLoad(transaction::Methods* trx) = 0;
  
  virtual uint8_t const* lookupRevisionVPack(TRI_voc_rid_t revisionId) const = 0;
  virtual uint8_t const* lookupRevisionVPackConditional(TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal) const = 0;
  virtual void insertRevision(TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid, bool isInWal, bool shouldLock) = 0;
  virtual void updateRevision(TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid, bool isInWal) = 0;
  virtual bool updateRevisionConditional(TRI_voc_rid_t revisionId, TRI_df_marker_t const* oldPosition, TRI_df_marker_t const* newPosition, TRI_voc_fid_t newFid, bool isInWal) = 0;
  virtual void removeRevision(TRI_voc_rid_t revisionId, bool updateStats) = 0;

  virtual bool isFullyCollected() const = 0;

  virtual int read(transaction::Methods*, arangodb::velocypack::Slice const key,
                   ManagedDocumentResult& result, bool) = 0;

  virtual int insert(arangodb::transaction::Methods* trx,
                     arangodb::velocypack::Slice const newSlice,
                     arangodb::ManagedDocumentResult& result,
                     OperationOptions& options,
                     TRI_voc_tick_t& resultMarkerTick, bool lock) = 0;

  virtual int update(arangodb::transaction::Methods* trx,
                     VPackSlice const newSlice, ManagedDocumentResult& result,
                     OperationOptions& options,
                     TRI_voc_tick_t& resultMarkerTick, bool lock,
                     TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                     TRI_voc_rid_t const& revisionId,
                     arangodb::velocypack::Slice const key) = 0;

  virtual int replace(transaction::Methods* trx,
                      arangodb::velocypack::Slice const newSlice,
                      ManagedDocumentResult& result, OperationOptions& options,
                      TRI_voc_tick_t& resultMarkerTick, bool lock,
                      TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                      TRI_voc_rid_t const revisionId,
                      arangodb::velocypack::Slice const fromSlice,
                      arangodb::velocypack::Slice const toSlice) = 0;

  virtual int remove(arangodb::transaction::Methods* trx,
                     arangodb::velocypack::Slice const slice,
                     arangodb::ManagedDocumentResult& previous,
                     OperationOptions& options,
                     TRI_voc_tick_t& resultMarkerTick, bool lock,
                     TRI_voc_rid_t const& revisionId, TRI_voc_rid_t& prevRev,
                     arangodb::velocypack::Slice const toRemove) = 0;

  virtual int removeFastPath(arangodb::transaction::Methods* trx,
                             TRI_voc_rid_t oldRevisionId,
                             arangodb::velocypack::Slice const oldDoc,
                             OperationOptions& options,
                             TRI_voc_tick_t& resultMarkerTick, bool lock,
                             TRI_voc_rid_t const& revisionId,
                             arangodb::velocypack::Slice const toRemove) = 0;

 protected:

  /// @brief merge two objects for update
  void mergeObjectsForUpdate(transaction::Methods* trx,
                             velocypack::Slice const& oldValue,
                             velocypack::Slice const& newValue,
                             bool isEdgeCollection, std::string const& rev,
                             bool mergeObjects, bool keepNull,
                             velocypack::Builder& builder);

  /// @brief new object for replace
  void newObjectForReplace(transaction::Methods* trx,
                           velocypack::Slice const& oldValue,
                           velocypack::Slice const& newValue,
                           velocypack::Slice const& fromSlice,
                           velocypack::Slice const& toSlice,
                           bool isEdgeCollection, std::string const& rev,
                           velocypack::Builder& builder);
 
 protected:
  LogicalCollection* _logicalCollection;
};

} // namespace arangodb

#endif
