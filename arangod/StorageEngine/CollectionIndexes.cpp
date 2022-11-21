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

#include "CollectionIndexes.h"

#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"
#include "Logger/LogMacros.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <absl/strings/str_cat.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

CollectionIndexes::ReadLocked::ReadLocked(
    basics::ReadWriteLock& lock, std::atomic<std::thread::id>& owner,
    CollectionIndexes::IndexContainerType const& indexes)
    : _locker(lock, owner, __FILE__, __LINE__), _indexes(indexes) {}

CollectionIndexes::IndexContainerType const&
CollectionIndexes::ReadLocked::indexes() const {
  return _indexes;
}

size_t CollectionIndexes::ReadLocked::size() const noexcept {
  return _indexes.size();
}

bool CollectionIndexes::ReadLocked::empty() const noexcept {
  return _indexes.empty();
}

CollectionIndexes::WriteLocked::WriteLocked(
    basics::ReadWriteLock& lock, std::atomic<std::thread::id>& owner,
    CollectionIndexes::IndexContainerType& indexes)
    : _locker(lock, owner, basics::LockerType::BLOCKING, true, __FILE__,
              __LINE__),
      _indexes(indexes) {
  TRI_ASSERT(_locker.isLocked());
}

std::shared_ptr<Index> CollectionIndexes::WriteLocked::lookup(
    velocypack::Slice info) const {
  TRI_ASSERT(info.isObject());

  auto value = info.get(StaticStrings::IndexType);  // extract type

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid index type definition");
  }

  Index::IndexType type = Index::type(value.stringView());

  return findIndex(
      [type, info](std::shared_ptr<Index> const& idx) {
        // directly return for tll indexes, as we allow at most one ttl index
        // per collection
        return (idx->type() == type) &&
               (type == arangodb::Index::IndexType::TRI_IDX_TYPE_TTL_INDEX ||
                idx->matchesDefinition(info));
      },
      _indexes);
}

std::shared_ptr<Index> CollectionIndexes::WriteLocked::lookup(
    IndexId idxId) const {
  return findIndex(
      [idxId](std::shared_ptr<Index> const& idx) { return idx->id() == idxId; },
      _indexes);
}

void CollectionIndexes::WriteLocked::add(std::shared_ptr<Index> const& idx) {
  CollectionIndexes::add(idx, _indexes);
}

std::shared_ptr<Index> CollectionIndexes::WriteLocked::remove(IndexId id) {
  return CollectionIndexes::remove(id, _indexes);
}

bool CollectionIndexes::IndexOrder::operator()(
    std::shared_ptr<Index> const& left,
    std::shared_ptr<Index> const& right) const {
  // Primary index always first (but two primary indexes render comparison
  // invalid but that`s a bug itself)
  TRI_ASSERT(
      !((left->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) &&
        (right->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX)));
  if (left->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    return true;
  } else if (right->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
    return false;
  }

  // edge indexes should go right after primary
  if (left->type() != right->type()) {
    if (left->type() == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
      return true;
    } else if (right->type() == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
      return false;
    }
  }

  // This failpoint allows CRUD tests to trigger reversal
  // of index operations. Hash index placed always AFTER reversable indexes
  // could be broken by unique constraint violation or by intentional failpoint.
  // And this will make possible to deterministically trigger index reversals
  TRI_IF_FAILURE("HashIndexAlwaysLast") {
    if (left->type() != right->type()) {
      if (left->type() == Index::IndexType::TRI_IDX_TYPE_HASH_INDEX) {
        return false;
      } else if (right->type() == Index::IndexType::TRI_IDX_TYPE_HASH_INDEX) {
        return true;
      }
    }
  }

  // indexes which needs no reverse should be done first to minimize
  // need for reversal procedures
  if (left->needsReversal() != right->needsReversal()) {
    return right->needsReversal();
  }
  // use id to make  order of equally-sorted indexes deterministic
  return left->id() < right->id();
}

/// @brief Find index by definition
std::shared_ptr<Index> CollectionIndexes::lookup(velocypack::Slice info) const {
  TRI_ASSERT(info.isObject());

  auto value = info.get(StaticStrings::IndexType);  // extract type

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid index type definition");
  }

  Index::IndexType type = Index::type(value.stringView());

  return findIndex([type, info](std::shared_ptr<Index> const& idx) {
    // directly return for tll indexes, as we allow at most one ttl index per
    // collection
    return (idx->type() == type) &&
           (type == arangodb::Index::IndexType::TRI_IDX_TYPE_TTL_INDEX ||
            idx->matchesDefinition(info));
  });
}

std::shared_ptr<Index> CollectionIndexes::lookup(Index::IndexType type) const {
  return findIndex([type](std::shared_ptr<Index> const& idx) {
    return idx->type() == type;
  });
}

std::shared_ptr<Index> CollectionIndexes::lookup(IndexId idxId) const {
  return findIndex([idxId](std::shared_ptr<Index> const& idx) {
    return idx->id() == idxId;
  });
}

std::shared_ptr<Index> CollectionIndexes::lookup(
    std::string_view idxName) const {
  return findIndex([idxName](std::shared_ptr<Index> const& idx) {
    return idx->name() == idxName;
  });
}

/// @brief hands out a list of indexes
std::vector<std::shared_ptr<Index>> CollectionIndexes::getAll() const {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  return {_indexes.begin(), _indexes.end()};
}

void CollectionIndexes::toVelocyPack(
    VPackBuilder& result,
    std::function<bool(Index const*,
                       std::underlying_type<Index::Serialize>::type&)> const&
        filter) const {
  result.openArray();
  {
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
    for (auto const& idx : _indexes) {
      std::underlying_type<Index::Serialize>::type flags = Index::makeFlags();

      if (!filter(idx.get(), flags)) {
        continue;
      }

      idx->toVelocyPack(result, flags);
    }
  }
  result.close();
}

Result CollectionIndexes::checkConflicts(
    std::shared_ptr<Index> const& newIdx) const {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);

  for (auto const& other : _indexes) {
    if (other->id() == newIdx->id() || other->name() == newIdx->name()) {
      // definition shares an identifier with an existing index with a
      // different definition
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      VPackBuilder builder1;
      newIdx->toVelocyPack(
          builder1, static_cast<std::underlying_type<Index::Serialize>::type>(
                        Index::Serialize::Basics));
      VPackBuilder builder2;
      other->toVelocyPack(
          builder2, static_cast<std::underlying_type<Index::Serialize>::type>(
                        Index::Serialize::Basics));
      LOG_TOPIC("29d1c", WARN, Logger::ENGINES)
          << "attempted to create index '" << builder1.slice().toJson()
          << "' but found conflicting index '" << builder2.slice().toJson()
          << "'";
#endif
      return {TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
              absl::StrCat("duplicate value for `", StaticStrings::IndexId,
                           "` or `", StaticStrings::IndexName, "`")};
    }
  }

  return {};
}

std::shared_ptr<Index> CollectionIndexes::findIndex(
    std::function<bool(std::shared_ptr<Index> const&)> const& cb) const {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  return findIndex(cb, _indexes);
}

/*static*/ std::shared_ptr<Index> CollectionIndexes::findIndex(
    std::function<bool(std::shared_ptr<Index> const&)> const& cb,
    IndexContainerType const& indexes) {
  for (auto const& idx : indexes) {
    if (cb(idx)) {
      return idx;
    }
  }
  return nullptr;
}

/// returns a pair of (num indexes, size of indexes)
std::pair<size_t, size_t> CollectionIndexes::stats() const {
  size_t numIndexes = 0;
  size_t sizeIndexes = 0;
  bool seenEdgeIndex = false;

  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  for (auto const& idx : _indexes) {
    // only count a single edge index instance
    if (idx->type() != Index::TRI_IDX_TYPE_EDGE_INDEX || !seenEdgeIndex) {
      ++numIndexes;
    }
    if (idx->type() == Index::TRI_IDX_TYPE_EDGE_INDEX) {
      seenEdgeIndex = true;
    }
    sizeIndexes += static_cast<size_t>(idx->memory());
  }

  return std::make_pair(numIndexes, sizeIndexes);
}

void CollectionIndexes::prepare(velocypack::Slice indexesSlice,
                                LogicalCollection& collection,
                                IndexFactory const& indexFactory) {
  std::vector<std::shared_ptr<Index>> indexes;

  RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);

  // link creation needs read-lock too
  if (indexesSlice.length() == 0 && _indexes.empty()) {
    indexFactory.fillSystemIndexes(collection, indexes);
  } else {
    indexFactory.prepareIndexes(collection, indexesSlice, indexes);
  }

  TRI_ASSERT(_indexes.empty());
  for (std::shared_ptr<Index>& idx : indexes) {
    TRI_ASSERT(idx != nullptr);
    auto const id = idx->id();
    for (auto const& it : _indexes) {
      TRI_ASSERT(it != nullptr);
      if (it->id() == id) {  // index is there twice
        idx.reset();
        break;
      }
    }

    if (idx) {
      TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id.id()));
      _indexes.emplace(idx);
      TRI_ASSERT(idx->type() != Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
                 idx->id().isPrimary());
    }
  }

  auto it = _indexes.cbegin();
  if ((*it)->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX ||
      (TRI_COL_TYPE_EDGE == collection.type() &&
       (_indexes.size() < 3 ||
        ((*++it)->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX ||
         (*++it)->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX)))) {
    std::string msg =
        "got invalid indexes for collection '" + collection.name() + "'";
    LOG_TOPIC("0ef34", ERR, arangodb::Logger::ENGINES) << msg;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    for (auto const& it : _indexes) {
      LOG_TOPIC("19e0b", ERR, arangodb::Logger::ENGINES)
          << "- " << it->context();
    }
#endif
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }

  TRI_ASSERT(!_indexes.empty());
}

void CollectionIndexes::add(std::shared_ptr<Index> const& idx) {
  RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
  CollectionIndexes::add(idx, _indexes);
}

std::shared_ptr<Index> CollectionIndexes::remove(IndexId id) {
  RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
  return CollectionIndexes::remove(id, _indexes);
}

void CollectionIndexes::replace(IndexId id,
                                std::shared_ptr<Index> const& newIdx) {
  RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
  CollectionIndexes::remove(id, _indexes);
  CollectionIndexes::add(newIdx, _indexes);
}

CollectionIndexes::ReadLocked CollectionIndexes::readLocked() const {
  return ReadLocked(_indexesLock, _indexesLockWriteOwner, _indexes);
}

CollectionIndexes::WriteLocked CollectionIndexes::writeLocked() {
  return WriteLocked(_indexesLock, _indexesLockWriteOwner, _indexes);
}

void CollectionIndexes::unload() {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  for (auto& it : _indexes) {
    it->unload();
  }
}

void CollectionIndexes::drop() {
  RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
  _indexes.clear();
}

/*static*/ void CollectionIndexes::add(std::shared_ptr<Index> const& idx,
                                       IndexContainerType& indexes) {
  indexes.emplace(idx);
}

/*static*/ std::shared_ptr<Index> CollectionIndexes::remove(
    IndexId id, IndexContainerType& indexes) {
  auto it = indexes.begin();
  while (it != indexes.end()) {
    if ((*it)->id() == id &&
        (*it)->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
      std::shared_ptr<Index> removed = (*it);
      indexes.erase(it);
      return removed;
    }
    ++it;
  }

  return nullptr;
}

}  // namespace arangodb
