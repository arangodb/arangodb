////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <map>
#include <mutex>
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

  RocksDBMetadata(RocksDBMetadata const&) = delete;
  RocksDBMetadata operator=(RocksDBMetadata const&) = delete;

  /// @brief collection count
  struct DocCount {
    rocksdb::SequenceNumber
        _committedSeq;       /// safe sequence number for recovery
    uint64_t _added;         /// number of added documents
    uint64_t _removed;       /// number of removed documents
    RevisionId _revisionId;  /// @brief last used revision id

    DocCount(rocksdb::SequenceNumber sq, uint64_t added, uint64_t removed,
             RevisionId rid)
        : _committedSeq(sq),
          _added(added),
          _removed(removed),
          _revisionId(rid) {}

    explicit DocCount(arangodb::velocypack::Slice const&);
    void toVelocyPack(arangodb::velocypack::Builder&) const;
  };

 public:
  RocksDBMetadata();

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // marks document counts as tainted during testing
  void setTainted() { _tainted = true; }
#endif

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool tainted() const noexcept {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    // if we use failure tests, the document counts may have been intentionally
    // corrupted. the tainted state is set by the failure points that corrupt
    // the counters.
    return _tainted;
#else
    // if we don't use failure tests, the document counts are never tainted.
    return false;
#endif
  }
#else
  // non-maintainer mode...
  static constexpr bool tainted() noexcept { return false; }
#endif

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
   */
  rocksdb::SequenceNumber placeBlocker(TransactionId trxId,
                                       rocksdb::SequenceNumber seq);

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
  void removeBlocker(TransactionId trxId) noexcept;

  /// @brief check if there is blocker with a seq number lower or equal to
  /// the specified number
  bool hasBlockerUpTo(rocksdb::SequenceNumber seq) const noexcept;

  /// @brief returns the largest safe seq to squash updates against
  rocksdb::SequenceNumber committableSeq(
      rocksdb::SequenceNumber maxCommitSeq) const;

  /// @brief buffer a counter adjustment
  void adjustNumberDocuments(rocksdb::SequenceNumber seq, RevisionId revId,
                             int64_t adj);

  /// @brief buffer a counter adjustment ONLY in recovery, optimized to use less
  /// memory
  void adjustNumberDocumentsInRecovery(rocksdb::SequenceNumber seq,
                                       RevisionId revId, int64_t adj);

  /// @brief serialize the collection metadata
  arangodb::Result serializeMeta(rocksdb::WriteBatch&, LogicalCollection&,
                                 bool force, arangodb::velocypack::Builder&,
                                 rocksdb::SequenceNumber& appliedSeq,
                                 std::string& output);

  /// @brief deserialize collection metadata, only called on startup
  arangodb::Result deserializeMeta(rocksdb::DB*, LogicalCollection&);

  void loadInitialNumberDocuments();

  uint64_t numberDocuments() const noexcept {
    return _numberDocuments.load(std::memory_order_acquire);
  }

  rocksdb::SequenceNumber countCommitted() const noexcept {
    return _count._committedSeq;
  }

  RevisionId revisionId() const noexcept {
    return _revisionId.load(std::memory_order_acquire);
  }

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

  mutable arangodb::basics::ReadWriteLock _blockerLock;
  /// @brief blocker identifies a transaction being committed
  // TODO we should probably use flat_map or abseils Swiss Tables
  std::map<TransactionId, rocksdb::SequenceNumber> _blockers;
  std::set<std::pair<rocksdb::SequenceNumber, TransactionId>> _blockersBySeq;
  rocksdb::SequenceNumber _maxBlockersSequenceNumber;

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

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // whether document counts are tainted during testing
  bool _tainted = false;
#endif
};

/// helper class for acquiring and releasing a blocker.
/// constructing an object of this class will do nothing, but once
/// placeBlocker() is called, the object takes care of releasing the
/// blocker upon destruction. An acquired blocker can also be released
/// prematurely by calling releaseBlocker().
class RocksDBBlockerGuard {
 public:
  explicit RocksDBBlockerGuard(LogicalCollection* collection);
  ~RocksDBBlockerGuard();
  RocksDBBlockerGuard(RocksDBBlockerGuard const&) = delete;
  RocksDBBlockerGuard& operator=(RocksDBBlockerGuard const&) = delete;
  RocksDBBlockerGuard(RocksDBBlockerGuard&&) noexcept;
  RocksDBBlockerGuard& operator=(RocksDBBlockerGuard&&) noexcept;

  /// @brief place a blocker without prescribing a transaction id.
  /// it is not allowed to call placeBlocker() if a blocker is already
  /// acquired by the object.
  rocksdb::SequenceNumber placeBlocker();

  /// @brief place a blocker for a specific transaction id.
  /// it is not allowed to call placeBlocker() if a blocker is already
  /// acquired by the object.
  rocksdb::SequenceNumber placeBlocker(TransactionId id);

  /// @brief releases an acquired blocker. will do nothing if no
  /// blocker is currently acquired by the object.
  void releaseBlocker() noexcept;

 private:
  LogicalCollection* _collection;
  TransactionId _trxId;
};

}  // namespace arangodb
