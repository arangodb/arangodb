////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COLLECTION_META_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COLLECTION_META_H 1

#include <mutex>
#include <map>
#include <set>

#include <rocksdb/types.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/Identifiers/TransactionId.h"
#include "VocBase/voc-types.h"

namespace rocksdb {
class DB;
class WriteBatch;
}  // namespace rocksdb

namespace arangodb {

class LogicalCollection;
class RocksDBRecoveryManager;

/// @brief metadata used by the index estimates and collection counts
struct RocksDBMetadata final {
  friend class RocksDBRecoveryManager;

  /// @brief collection count
  struct DocCount {
    rocksdb::SequenceNumber _committedSeq; /// safe sequence number for recovery
    uint64_t _added; /// number of added documents
    uint64_t _removed; /// number of removed documents
    RevisionId _revisionId;  /// @brief last used revision id

    DocCount(rocksdb::SequenceNumber sq, uint64_t added, uint64_t removed, RevisionId rid)
        : _committedSeq(sq), _added(added), _removed(removed), _revisionId(rid) {}

    explicit DocCount(arangodb::velocypack::Slice const&);
    void toVelocyPack(arangodb::velocypack::Builder&) const;
  };

 public:
  RocksDBMetadata();

 public:
  /**
   * @brief Place a blocker to allow proper commit/serialize semantics
   *
   * Should be called immediately prior to beginning an internal trx. If the
   * trx commit succeeds, any inserts/removals should be buffered, then the
   * blocker updated (intermediate) or removed (final); otherwise simply remove
   * the blocker.
   *
   * @param  trxId The identifier for the active transaction
   * @param  seq   The sequence number immediately prior to call
   * @return       May return error if we fail to allocate and place blocker
   */
  Result placeBlocker(TransactionId trxId, rocksdb::SequenceNumber seq);

  /**
   * @brief Update a blocker to allow proper commit/serialize semantics
   *
   * Should be called after initializing an internal trx.
   *
   * @param  trxId The identifier for the active transaction (should match input
   *               to earlier `placeBlocker` call)
   * @param  seq   The sequence number from the internal snapshot
   * @return       May return error if we fail to allocate and place blocker
   */
  Result updateBlocker(TransactionId trxId, rocksdb::SequenceNumber seq);

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
  void removeBlocker(TransactionId trxId);

  /// @brief returns the largest safe seq to squash updates against
  rocksdb::SequenceNumber committableSeq(rocksdb::SequenceNumber maxCommitSeq) const;

  /// @brief buffer a counter adjustment
  void adjustNumberDocuments(rocksdb::SequenceNumber seq, RevisionId revId, int64_t adj);

  /// @brief buffer a counter adjustment ONLY in recovery, optimized to use less memory
  void adjustNumberDocumentsInRecovery(rocksdb::SequenceNumber seq,
                                       RevisionId revId, int64_t adj);

  /// @brief serialize the collection metadata
  arangodb::Result serializeMeta(rocksdb::WriteBatch&, LogicalCollection&,
                                 bool force, arangodb::velocypack::Builder&,
                                 rocksdb::SequenceNumber& appliedSeq, std::string& output);

  /// @brief deserialize collection metadata, only called on startup
  arangodb::Result deserializeMeta(rocksdb::DB*, LogicalCollection&);
  
  void loadInitialNumberDocuments();

  uint64_t numberDocuments() const {
    return _numberDocuments.load(std::memory_order_acquire);
  }

  rocksdb::SequenceNumber countCommitted() const {
    return _count._committedSeq;
  }

  RevisionId revisionId() const {
    return _revisionId.load(std::memory_order_acquire);
  }

public:
  // static helper methods to modify collection meta entries in rocksdb

  /// @brief load collection document count
  static DocCount loadCollectionCount(rocksdb::DB*, uint64_t objectId);

  /// @brief remove collection metadata
  static Result deleteCollectionMeta(rocksdb::DB*, uint64_t objectId);

  /// @brief remove collection index estimate
  static Result deleteIndexEstimate(rocksdb::DB*, uint64_t objectId);

 private:
  /// @brief apply counter adjustments, only call from sync thread
  bool applyAdjustments(rocksdb::SequenceNumber commitSeq);

 private:
  // TODO we should probably use flat_map or abseils Swiss Tables

  mutable arangodb::basics::ReadWriteLock _blockerLock;
  /// @brief blocker identifies a transaction being committed
  std::map<TransactionId, rocksdb::SequenceNumber> _blockers;
  std::set<std::pair<rocksdb::SequenceNumber, TransactionId>> _blockersBySeq;

  DocCount _count;  /// @brief document count struct

  /// document counter adjustment
  struct Adjustment {
    /// @brief last used revision id
    RevisionId revisionId;
    /// @brief number of added / removed documents
    int64_t adjustment;
  };

  mutable std::mutex _bufferLock;
  /// @brief buffered counter adjustments
  std::map<rocksdb::SequenceNumber, Adjustment> _bufferedAdjs;
  /// @brief internal buffer for adjustments
  std::map<rocksdb::SequenceNumber, Adjustment> _stagedAdjs;
  
  // below values are updated immediately, but are not serialized
  std::atomic<uint64_t> _numberDocuments;
  std::atomic<RevisionId> _revisionId;
};
}  // namespace arangodb

#endif
