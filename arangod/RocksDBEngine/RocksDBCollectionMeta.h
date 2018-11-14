////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COLLECTION_META_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COLLECTION_META_H 1

#include "Basics/Result.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/voc-types.h"

#include <mutex>

#include <rocksdb/types.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace rocksdb {
  class DB;
  class WriteBatch;
}

namespace arangodb {
  
class LogicalCollection;
class RocksDBCollection;
class RocksDBRecoveryManager;

/// @brief metadata used by the index estimates and collection counts
/// transaction to verify
struct RocksDBCollectionMeta final {
  friend class RocksDBRecoveryManager;
 
  /// @brief collection count
  struct DocCount {
    /// @brief safe sequence number for recovery
    rocksdb::SequenceNumber _committedSeq;
    /// @brief number of added documents
    uint64_t _added;
    /// @brief number of removed documents
    uint64_t _removed;
    /// @brief last used revision id
    TRI_voc_rid_t _revisionId;
    
    DocCount(rocksdb::SequenceNumber sq, uint64_t added,
             uint64_t removed, TRI_voc_rid_t rid)
    : _committedSeq(sq), _added(added), _removed(removed), _revisionId(rid) {}
    
    explicit DocCount(arangodb::velocypack::Slice const&);
    void toVelocyPack(arangodb::velocypack::Builder&) const;
  };
  
 public:
  
  RocksDBCollectionMeta();
  
 public:
  /**
   * @brief Place a blocker to allow proper commit/serialize semantics
   *
   * Should be called immediately prior to internal RocksDB commit. If the
   * commit succeeds, any inserts/removals should be buffered, then the blocker
   * removed; otherwise simply remove the blocker.
   *
   * @param  trxId The identifier for the active transaction
   * @param  seq   The sequence number immediately prior to call
   * @return       May return error if we fail to allocate and place blocker
   */
  Result placeBlocker(uint64_t trxId, rocksdb::SequenceNumber seq);
  
  /**
   * @brief Removes an existing transaction blocker
   *
   * Should be called after transaction abort/rollback, or after buffering any
   * updates in case of successful commit. If no blocker exists with the
   * specified transaction identifier, then this will simply do nothing.
   *
   * @param trxId Identifier for active transaction (should match input to
   *              earlier `placeBlocker` call)
   */
  void removeBlocker(uint64_t trxId);
  
  /// @brief updates and returns the largest safe seq to squash updated against
  rocksdb::SequenceNumber committableSeq() const;
  
  /// @brief get the current count
  DocCount currentCount();
  /// @brief get the current count, ONLY use in recovery
  DocCount& countRefUnsafe() { return _count; }
  
  /// @brief buffer a counter adjustment
  void adjustNumberDocuments(rocksdb::SequenceNumber seq,
                             TRI_voc_rid_t revId, int64_t adj);

  /// @brief serialize the collection metadata
  arangodb::Result serializeMeta(rocksdb::WriteBatch&, LogicalCollection&,
                                 bool force, arangodb::velocypack::Builder&,
                                 rocksdb::SequenceNumber& appliedSeq);
  
  /// @brief deserialize collection metadata, only called on startup
  arangodb::Result deserializeMeta(rocksdb::DB*, LogicalCollection&);
  
  /// @brief load collection
  static DocCount loadCollectionCount(rocksdb::DB*, uint64_t objectId);
  
  /// @brief remove collection metadata
  static Result deleteCollectionMeta(rocksdb::DB*, uint64_t objectId);
  
  /// @brief remove collection index estimate
  static Result deleteIndexEstimate(rocksdb::DB*, uint64_t objectId);

private:
  
  /// @brief apply counter adjustments, only call from sync thread
  rocksdb::SequenceNumber applyAdjustments(rocksdb::SequenceNumber commitSeq,
                                           bool& didWork);
  
private:

  // TODO we should probably use flat_map or abseils Swiss Tables
  
  mutable arangodb::basics::ReadWriteLock _blockerLock;
  /// @brief blocker identifies a transaction being committed
  std::map<uint64_t, rocksdb::SequenceNumber> _blockers;
  std::set<std::pair<rocksdb::SequenceNumber, uint64_t>> _blockersBySeq;
  
  mutable std::mutex _countLock;
  DocCount _count; /// @brief document count struct
  
  /// document counter adjustment
  struct Adjustment {
    /// @brief last used revision id
    TRI_voc_rid_t revisionId;
    /// @brief number of added / removed documents
    int64_t adjustment;
  };
  
  /// @brief buffered counter adjustments
  std::map<rocksdb::SequenceNumber, Adjustment> _bufferedAdjs;
  /// @brief internal buffer for adjustments
  std::map<rocksdb::SequenceNumber, Adjustment> _stagedAdjs;
};
}

#endif
