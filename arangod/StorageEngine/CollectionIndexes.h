////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/Result.h"
#include "Indexes/Index.h"

namespace arangodb {
class IndexFactory;
class LogicalCollection;

namespace velocypack {
class Slice;
}

class CollectionIndexes {
 public:
  /// @brief determines order of index execution on collection
  struct IndexOrder {
    bool operator()(std::shared_ptr<Index> const& left,
                    std::shared_ptr<Index> const& right) const;
  };

  using IndexContainerType = std::set<std::shared_ptr<Index>, IndexOrder>;

  class ReadLocked {
   public:
    ReadLocked(basics::ReadWriteLock& lock, std::atomic<std::thread::id>& owner,
               IndexContainerType const& indexes);

    IndexContainerType const& indexes() const;

    size_t size() const noexcept;
    bool empty() const noexcept;

   private:
    RecursiveReadLocker<basics::ReadWriteLock> _locker;
    IndexContainerType const& _indexes;
  };

  class WriteLocked {
   public:
    WriteLocked(basics::ReadWriteLock& lock,
                std::atomic<std::thread::id>& owner,
                IndexContainerType& indexes);

    std::shared_ptr<Index> lookup(velocypack::Slice info) const;
    std::shared_ptr<Index> lookup(IndexId idxId) const;
    void add(std::shared_ptr<Index> const& idx);
    std::shared_ptr<Index> remove(IndexId id);

   private:
    RecursiveWriteLocker<basics::ReadWriteLock> _locker;
    IndexContainerType& _indexes;
  };

  // find index by definition. returns index or nullptr
  std::shared_ptr<Index> lookup(velocypack::Slice info) const;

  // find index by type. returns index or nullptr
  std::shared_ptr<Index> lookup(Index::IndexType type) const;

  // find index by id. returns index or nullptr
  std::shared_ptr<Index> lookup(IndexId idxId) const;

  // find index by name. returns index or nullptr
  std::shared_ptr<Index> lookup(std::string_view idxName) const;

  // find index by filter callback. returns index or nullptr
  std::shared_ptr<Index> findIndex(
      std::function<bool(std::shared_ptr<Index> const&)> const& cb) const;

  // return all indexes of the collection
  std::vector<std::shared_ptr<Index>> getAll() const;

  // velocypack-ify all indexes of the collection
  void toVelocyPack(
      velocypack::Builder&,
      std::function<bool(Index const*,
                         std::underlying_type<Index::Serialize>::type&)> const&
          filter) const;

  Result checkConflicts(std::shared_ptr<Index> const& newIdx) const;

  // returns a pair of (num indexes, size of indexes) cumulated for all
  // indexes of the collection
  std::pair<size_t, size_t> stats() const;

  void prepare(velocypack::Slice indexesSlice, LogicalCollection& collection,
               IndexFactory const& indexFactory);

  // add a new index. index definition must have been validated externally
  // before
  void add(std::shared_ptr<Index> const& idx);

  // remove an index from the collection. returns nullptr if no such index
  // existed
  std::shared_ptr<Index> remove(IndexId id);

  // replaces index with id with newIdx
  void replace(IndexId id, std::shared_ptr<Index> const& newIdx);

  // returns a ReadLocked RAII object which holds the read-lock on the list
  // of indexes for the collection. while that object is in the scope, the
  // list of indexes is guaranteed to be read-locked. note: this does not lock
  // the indexes themselves, i.e. inserting into/removing from indexes is
  // still possible - simply DDL operations on indexes are blocked
  ReadLocked readLocked() const;

  // returns a WriteLocked RAII object which holds the write-lock on the list
  // of indexes for the collection. while that object is in the scope, the
  // list of indexes is guaranteed to be write-locked
  WriteLocked writeLocked();

  // unload all indexes of the collection, without destroying them
  void unload();

  // drops all indexes of the collection, should be called only during
  // shutdown or when a collection is dropped
  void drop();

 private:
  static std::shared_ptr<Index> findIndex(
      std::function<bool(std::shared_ptr<Index> const&)> const& cb,
      IndexContainerType const& indexes);

  static std::shared_ptr<Index> remove(IndexId id, IndexContainerType& indexes);
  static void add(std::shared_ptr<Index> const& idx,
                  IndexContainerType& indexes);

  mutable basics::ReadWriteLock _indexesLock;
  // current thread owning '_indexesLock'
  mutable std::atomic<std::thread::id> _indexesLockWriteOwner;
  IndexContainerType _indexes;
};

}  // namespace arangodb
