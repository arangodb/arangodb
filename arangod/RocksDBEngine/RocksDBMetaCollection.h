////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Basics/FutureSharedLock.h"
#include "Basics/UnshackledMutex.h"
#include "Basics/ResultT.h"
#include "Containers/MerkleTree.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMetadata.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <type_traits>

namespace arangodb {
class RevisionReplicationIterator;
class RocksDBEngine;

class RocksDBMetaCollection : public PhysicalCollection {
 public:
  explicit RocksDBMetaCollection(LogicalCollection& collection,
                                 velocypack::Slice info);
  virtual ~RocksDBMetaCollection();

  void deferDropCollection(
      std::function<bool(LogicalCollection&)> const&) override;

  uint64_t objectId() const noexcept { return _objectId; }

  RocksDBMetadata& meta() { return _meta; }
  RocksDBMetadata const& meta() const noexcept { return _meta; }

  RevisionId revision(arangodb::transaction::Methods* trx) const override final;
  uint64_t numberDocuments(transaction::Methods* trx) const override final;

  futures::Future<ErrorCode> lockWrite(double timeout = 0.0);
  void unlockWrite() noexcept;
  futures::Future<ErrorCode> lockRead(double timeout = 0.0);
  void unlockRead();

  void freeMemory() noexcept override;

  /// recalculate counts for collection in case of failure, blocks other writes
  /// for a short period
  futures::Future<uint64_t> recalculateCounts() override;

  /// @brief compact-data operation
  /// triggers rocksdb compaction for documentDB and indexes
  void compact() override final;

  /// estimate size of collection and indexes
  void estimateSize(velocypack::Builder& builder);

  void setRevisionTree(std::unique_ptr<containers::RevisionTree>&& tree,
                       uint64_t seq);
  std::unique_ptr<containers::RevisionTree> revisionTree(
      transaction::Methods& trx) override;
  std::unique_ptr<containers::RevisionTree> revisionTree(
      rocksdb::SequenceNumber trxSeq) override;
  std::unique_ptr<containers::RevisionTree> computeRevisionTree(
      uint64_t batchId) override;

  Result takeCareOfRevisionTreePersistence(
      LogicalCollection& coll, RocksDBEngine& engine,
      rocksdb::WriteBatch& batch, rocksdb::ColumnFamilyHandle* const cf,
      rocksdb::SequenceNumber maxCommitSeq, bool force,
      std::string const& context, std::string& output,
      rocksdb::SequenceNumber& appliedSeq);

  futures::Future<Result> rebuildRevisionTree() override;
  void rebuildRevisionTree(std::unique_ptr<rocksdb::Iterator>& iter);
  // returns a pair with the number of documents and the tree's seq number.
  std::pair<uint64_t, uint64_t> revisionTreeInfo() const;

  [[nodiscard]] futures::Future<futures::Unit> revisionTreeSummary(
      VPackBuilder& builder, bool fromCollection);
  void revisionTreePendingUpdates(VPackBuilder& builder);

  uint64_t placeRevisionTreeBlocker(TransactionId transactionId) override;
  void removeRevisionTreeBlocker(TransactionId transactionId) override;

  /**
   * @brief Buffer updates to this collection to be applied when appropriate
   *
   * Buffers updates associated with a given commit seq/tick. Will hold updates
   * until all previous blockers have been removed to ensure a consistent state
   * for sync/recovery and avoid any missed updates.
   *
   * @param  seq      The seq/tick post-commit, prior to call
   * @param  inserts  Vector of revisions to insert
   * @param  removals Vector of revisions to remove
   */
  void bufferUpdates(rocksdb::SequenceNumber seq,
                     std::vector<std::uint64_t>&& inserts,
                     std::vector<std::uint64_t>&& removals);

  Result bufferTruncate(rocksdb::SequenceNumber seq);

  /// @brief return bounds for all documents
  virtual RocksDBKeyBounds bounds() const = 0;

  /// @brief produce a revision tree from the documents in the collection
  futures::Future<ResultT<std::pair<std::unique_ptr<containers::RevisionTree>,
                                    rocksdb::SequenceNumber>>>
  revisionTreeFromCollection(bool checkForBlockers);

  std::unique_ptr<containers::RevisionTree> buildTreeFromIterator(
      RevisionReplicationIterator& it) const;

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  void corruptRevisionTree(std::uint64_t count, std::uint64_t hash);
#endif

 private:
  bool needToPersistRevisionTree(
      rocksdb::SequenceNumber maxCommitSeq,
      std::unique_lock<std::mutex> const& lock) const;
  rocksdb::SequenceNumber lastSerializedRevisionTree(
      rocksdb::SequenceNumber maxCommitSeq,
      std::unique_lock<std::mutex> const& lock);
  rocksdb::SequenceNumber serializeRevisionTree(
      std::string& output, rocksdb::SequenceNumber commitSeq, bool force,
      std::unique_lock<std::mutex> const& lock);

  /// @brief sends the collection's revision tree to hibernation
  void hibernateRevisionTree(std::unique_lock<std::mutex> const& lock);

  Result applyUpdatesForTransaction(containers::RevisionTree& tree,
                                    rocksdb::SequenceNumber commitSeq,
                                    std::unique_lock<std::mutex>& lock) const;

  futures::Future<ErrorCode> doLock(double timeout, AccessMode::Type mode);
  bool haveBufferedOperations(std::unique_lock<std::mutex> const& lock) const;
  std::unique_ptr<containers::RevisionTree> allocateEmptyRevisionTree(
      std::size_t depth) const;
  void applyUpdates(rocksdb::SequenceNumber commitSeq,
                    std::unique_lock<std::mutex> const& lock);
  void ensureRevisionTree();

  // helper function to build revision trees
  std::unique_ptr<containers::RevisionTree> revisionTree(
      rocksdb::SequenceNumber notAfter,
      std::function<std::unique_ptr<containers::RevisionTree>(
          std::unique_ptr<containers::RevisionTree>,
          std::unique_lock<std::mutex>& lock)> const& callback);

  // used for calculating memory usage in _revisionsBufferedMemoryUsage
  // approximate memory usage for top-level items.
  // approximate size of a single entry in _revisionInsertBuffers plus the
  // size of one allocation.
  static constexpr uint64_t bufferedEntrySize() {
    return sizeof(void*) + sizeof(decltype(_revisionInsertBuffers)::value_type);
  }
  // approximate memory usage for individual items in a top-level item,
  // i.e. size of _revisionInsertBuffers.second::value_type.
  // this does not take into account unused capacity in the
  // _revisionInsertBuffers.second.
  static constexpr uint64_t bufferedEntryItemSize() {
    return sizeof(decltype(_revisionInsertBuffers)::mapped_type::value_type);
  }

  void increaseBufferedMemoryUsage(uint64_t value) noexcept;
  void decreaseBufferedMemoryUsage(uint64_t value) noexcept;

 protected:
  RocksDBMetadata _meta;  /// collection metadata
  /// @brief collection lock used for write access

  struct SchedulerWrapper {
    using WorkHandle = Scheduler::WorkHandle;
    template<typename F>
    void queue(F&&);
    template<typename F>
    WorkHandle queueDelayed(F&&, std::chrono::milliseconds);
  };

  SchedulerWrapper _schedulerWrapper;
  using FutureLock = arangodb::futures::FutureSharedLock<SchedulerWrapper>;
  mutable FutureLock _exclusiveLock;
  /// @brief collection lock used for recalculation count values
  mutable basics::UnshackledMutex _recalculationLock;

  /// @brief depth for all revision trees.
  /// depth is large from the beginning so that the trees are always
  /// large enough to handle large collections and do not need resizing.
  /// as the combined RAM usage for all such trees would be prohibitive,
  /// we may hold some of the trees in memory only in a compressed variant.
  static constexpr std::size_t revisionTreeDepth = 6;

 private:
  RocksDBEngine& _engine;

  uint64_t const _objectId;  /// rocksdb-specific object id for collection

  /// @brief helper class for accessing revision trees in a compressed or
  /// uncompressed state. this class alone is not thread-safe. it must be
  /// used with external synchronization
  class RevisionTreeAccessor {
   public:
    RevisionTreeAccessor(RevisionTreeAccessor const&) = delete;
    RevisionTreeAccessor& operator=(RevisionTreeAccessor const&) = delete;

    explicit RevisionTreeAccessor(
        std::unique_ptr<containers::RevisionTree> tree,
        LogicalCollection const& collection);

    ~RevisionTreeAccessor();

    void insert(std::vector<std::uint64_t> const& keys);
    void remove(std::vector<std::uint64_t> const& keys);
    void clear();
    std::unique_ptr<containers::RevisionTree> clone() const;
    std::uint64_t count() const;
    std::uint64_t rootValue() const;
    std::uint64_t depth() const;
    std::size_t memoryUsage() const;

    // potentially expensive! only call when necessary
    std::uint64_t compressedSize() const;

    void checkConsistency() const;
    void serializeBinary(std::string& output) const;
    void delayCompression();

    // turn the full-blown revision tree into a potentially smaller
    // compressed representation
    void hibernate(bool force);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    void corrupt(std::uint64_t count, std::uint64_t hash);
#endif

   private:
    /// @brief make sure we have a value in _tree. unfortunately we
    /// need to declare this const although it can mutate the internal
    /// state of _tree
    void ensureTree() const;

    /// @brief compressed representation of tree. we will either have
    /// this attribute populated or _tree
    std::string mutable _compressed;

    /// @brief the actual tree. we will either have this populated or
    ///_compressed
    std::unique_ptr<containers::RevisionTree> mutable _tree;

    /// @brief collecion object, used only for context in log messages
    LogicalCollection const& _logicalCollection;

    /// @brief depth of tree. supposed to never change
    std::uint64_t const _depth;

    /// @brief number of hibernation requests received (we may ignore the
    /// first few ones)
    std::uint32_t mutable _hibernationRequests;

    /// @brief whether or not we should attempt to compress the tree
    bool _compressible;

    /// @brief when we last tried to hibernate/compress the revision tree
    std::chrono::time_point<
        std::chrono::steady_clock> mutable _lastHibernateAttempt;
  };

  // The following rules/definitions apply:
  //  a. A tree which is persisted under sequence number `S` reflects the
  //     state of the collection once all changes in the WAL up to and
  //     including sequence number `S` have been applied but no more.
  //  b. A transaction counts as applied, if the sequence number of its
  //     commit marker has been applied to the tree. The other operations
  //     have a smaller sequence number in the WAL.
  //
  // We must maintain the following invariants under all circumstances:
  //  1. `_revisionTreeCreationSeq` <= `_revisionTreeSerializedSeq`
  //  2. `_revisionTreeSerializedSeq` <= `_revisionTreeApplied`
  //  3. `_revisionTreeApplied` <= all blockers (in _meta) at all times
  //  4. There are no buffered changes with a sequence number less than
  //     or equal to `_revisionTreeApplied`.
  // If you change anything around the members below, or the blockers
  // in the `_meta` subobject, **please** make sure that these invariants
  // are maintained. If not, we are entering a world of pain! We have
  // been there and do not want to go back!

  /// @revision tree management for replication
  mutable std::mutex _revisionTreeLock;
  std::unique_ptr<RevisionTreeAccessor> _revisionTree;
  rocksdb::SequenceNumber _revisionTreeApplied;
  rocksdb::SequenceNumber _revisionTreeCreationSeq;
  rocksdb::SequenceNumber _revisionTreeSerializedSeq;
  std::chrono::steady_clock::time_point _revisionTreeSerializedTime;
  bool _revisionTreeCanBeSerialized = true;

  // if the types of these containers are changed to some other type, please
  // check the new type's iterator invalidation rules first and if iterators are
  // invalidated when new elements get inserted
  std::map<rocksdb::SequenceNumber, std::vector<std::uint64_t>>
      _revisionInsertBuffers;
  std::map<rocksdb::SequenceNumber, std::vector<std::uint64_t>>
      _revisionRemovalBuffers;

  // current memory accounting implementation requires both members to use the
  // same underlying data structures
  static_assert(std::is_same_v<decltype(_revisionInsertBuffers)::value_type,
                               decltype(_revisionRemovalBuffers)::value_type>);

  std::set<rocksdb::SequenceNumber> _revisionTruncateBuffer;

  uint64_t _revisionsBufferedMemoryUsage;
};

}  // namespace arangodb
