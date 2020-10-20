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

#pragma once
#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_META_COLLECTION_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_META_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Containers/MerkleTree.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMetadata.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {

class RocksDBMetaCollection : public PhysicalCollection {
 public:
  explicit RocksDBMetaCollection(LogicalCollection& collection,
                                 arangodb::velocypack::Slice const& info);
  RocksDBMetaCollection(LogicalCollection& collection,
                        PhysicalCollection const*);  // use in cluster only!!!!!
  virtual ~RocksDBMetaCollection() = default;
  
  std::string const& path() const override final;

  void deferDropCollection(std::function<bool(LogicalCollection&)> const&) override final;

  /// @brief report extra memory used by indexes etc.
  size_t memory() const override final { return 0; }
  uint64_t objectId() const { return _objectId.load(); }

  RocksDBMetadata& meta() { return _meta; }

  RevisionId revision(arangodb::transaction::Methods* trx) const override final;
  uint64_t numberDocuments(transaction::Methods* trx) const override final;
  
  int lockWrite(double timeout = 0.0);
  void unlockWrite();
  int lockRead(double timeout = 0.0);
  void unlockRead();
  
  /// recalculate counts for collection in case of failure, blocks other writes for a short period
  uint64_t recalculateCounts() override;
 
  /// @brief compact-data operation
  /// triggers rocksdb compaction for documentDB and indexes
  Result compact() override final;
  
  /// estimate size of collection and indexes
  void estimateSize(velocypack::Builder& builder);

  void setRevisionTree(std::unique_ptr<containers::RevisionTree>&& tree, uint64_t seq);
  std::unique_ptr<containers::RevisionTree> revisionTree(transaction::Methods& trx) override;
  std::unique_ptr<containers::RevisionTree> revisionTree(uint64_t batchId) override;

  bool needToPersistRevisionTree(rocksdb::SequenceNumber maxCommitSeq) const;
  rocksdb::SequenceNumber lastSerializedRevisionTree(rocksdb::SequenceNumber maxCommitSeq);
  rocksdb::SequenceNumber serializeRevisionTree(std::string& output,
                                                rocksdb::SequenceNumber commitSeq,
                                                bool force);

  Result rebuildRevisionTree() override;
  void rebuildRevisionTree(std::unique_ptr<rocksdb::Iterator>& iter);

  void revisionTreeSummary(VPackBuilder& builder);

  void placeRevisionTreeBlocker(TransactionId transactionId) override;
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
  void bufferUpdates(rocksdb::SequenceNumber seq, std::vector<std::uint64_t>&& inserts,
                     std::vector<std::uint64_t>&& removals);

  Result bufferTruncate(rocksdb::SequenceNumber seq);

 public:
  /// return bounds for all documents
  virtual RocksDBKeyBounds bounds() const = 0;
  
 protected:
  
  /// @brief track the usage of waitForSync option in an operation
  void trackWaitForSync(arangodb::transaction::Methods* trx, OperationOptions& options);

  void applyUpdates(rocksdb::SequenceNumber commitSeq);

  Result applyUpdatesForTransaction(containers::RevisionTree& tree,
                                    rocksdb::SequenceNumber commitSeq) const;

 private:
  int doLock(double timeout, AccessMode::Type mode);
  bool haveBufferedOperations() const;
  std::size_t revisionTreeDepth() const;
  std::unique_ptr<containers::RevisionTree> allocateEmptyRevisionTree() const;
  bool ensureRevisionTree();

 protected:
  RocksDBMetadata _meta;     /// collection metadata
  /// @brief collection lock used for write access
  mutable basics::ReadWriteLock _exclusiveLock;
  /// @brief collection lock used for recalculation count values
  mutable std::mutex _recalculationLock;

 private:
  std::atomic<uint64_t> _objectId;  /// rocksdb-specific object id for collection

  /// @revision tree management for replication
  std::unique_ptr<containers::RevisionTree> _revisionTree;
  std::atomic<rocksdb::SequenceNumber> _revisionTreeApplied;
  rocksdb::SequenceNumber _revisionTreeCreationSeq;
  rocksdb::SequenceNumber _revisionTreeSerializedSeq;
  std::chrono::steady_clock::time_point _revisionTreeSerializedTime;
  mutable std::mutex _revisionTreeLock;
  std::multimap<rocksdb::SequenceNumber, std::vector<std::uint64_t>> _revisionInsertBuffers;
  std::multimap<rocksdb::SequenceNumber, std::vector<std::uint64_t>> _revisionRemovalBuffers;
  std::set<rocksdb::SequenceNumber> _revisionTruncateBuffer;
  mutable std::mutex _revisionBufferLock;
};

} // namespace arangodb
  
#endif
