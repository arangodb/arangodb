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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <rocksdb/db.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "IResearch/IResearchRocksDBRecoveryHelper.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchRocksDBInvertedIndex.h"
#include "IResearch/IResearchRocksDBLink.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "Basics/DownCast.h"

#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>

namespace arangodb {
namespace transaction {

class Context;

}  // namespace transaction
namespace iresearch {

IResearchRocksDBRecoveryHelper::IResearchRocksDBRecoveryHelper(
    ArangodServer& server, std::span<std::string const> skipRecoveryItems)
    : _server{&server} {
  for (auto const& item : skipRecoveryItems) {
    if (item == "all") {
      _skipAllItems = true;
      _skipRecoveryItems = {};
      break;
    }
    std::pair<std::string_view, std::string_view> parts =
        absl::StrSplit(item, '/');
    _skipRecoveryItems[parts.first].emplace(parts.second);
  }
}

void IResearchRocksDBRecoveryHelper::prepare() {
  TRI_ASSERT(_server);
  _engine =
      &_server->getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  _dbFeature = &_server->getFeature<DatabaseFeature>();
  _documentCF = RocksDBColumnFamilyManager::get(
                    RocksDBColumnFamilyManager::Family::Documents)
                    ->GetID();
}

template<typename Impl>
bool IResearchRocksDBRecoveryHelper::skip(Impl& impl) {
  TRI_ASSERT(!impl.isOutOfSync());
  if (_skipAllItems) {
    return true;
  }
  auto it = _skipRecoveryItems.find(impl.Index::collection().name());
  return it != _skipRecoveryItems.end() &&
         (it->second.contains(impl.name()) ||
          it->second.contains(absl::AlphaNum{impl.Index::id().id()}.Piece()));
}

template<bool Force>
void IResearchRocksDBRecoveryHelper::clear() noexcept {
  if constexpr (!Force) {
    if (_indexes.size() < kMaxSize && _links.size() < kMaxSize &&
        _ranges.size() < kMaxSize) {
      return;
    }
  }
  _ranges.clear();
  _indexes.clear();
  _links.clear();
}

template<bool Exists, typename Func>
void IResearchRocksDBRecoveryHelper::applyCF(uint32_t column_family_id,
                                             rocksdb::Slice key,
                                             rocksdb::SequenceNumber tick,
                                             Func&& func) {
  if (column_family_id != _documentCF) {
    return;
  }

  auto const objectId = RocksDBKey::objectId(key);

  auto& ranges = getRanges(objectId);
  if (ranges.empty()) {
    return;
  }

  auto const documentId = RocksDBKey::documentId(key);

  auto apply = [&](auto range, auto& values, bool& needed) {
    if (range.empty()) {
      return;
    }
    auto begin = values.begin();
    auto it = begin + range.begin;
    auto end = range.end != kMaxSize ? begin + range.end : values.end();
    for (; it != end; ++it) {
      if (*it == nullptr) {
        continue;
      }
      if (tick <= (**it).recoveryTickLow()) {
        needed = true;
        continue;
      }
      if constexpr (Exists) {
        if (tick <= (**it).recoveryTickHigh() && (**it).exists(documentId)) {
          needed = true;
          continue;
        }
      }
      if (!skip(**it)) {
        func(**it, documentId);
        needed = true;
      } else {
        (**it).setOutOfSync();
        *it = nullptr;
      }
    }
  };

  bool indexes_needed = false;
  apply(ranges.indexes, _indexes, indexes_needed);

  bool links_needed = false;
  apply(ranges.links, _links, links_needed);

  if (!indexes_needed &&
      ranges.indexes.end >= static_cast<uint16_t>(_indexes.size())) {
    _indexes.resize(ranges.indexes.begin);
    ranges.indexes = {};
  }

  if (!links_needed &&
      ranges.links.end >= static_cast<uint16_t>(_links.size())) {
    _links.resize(ranges.links.begin);
    ranges.links = {};
  }
}

void IResearchRocksDBRecoveryHelper::PutCF(uint32_t column_family_id,
                                           rocksdb::Slice const& key,
                                           rocksdb::Slice const& value,
                                           rocksdb::SequenceNumber tick) {
  return applyCF<true>(column_family_id, key, tick,
                       [&](auto& impl, LocalDocumentId documentId) {
                         auto doc = RocksDBValue::data(value);
                         impl.recoveryInsert(tick, documentId, doc);
                       });
}

void IResearchRocksDBRecoveryHelper::DeleteCF(uint32_t column_family_id,
                                              rocksdb::Slice const& key,
                                              rocksdb::SequenceNumber tick) {
  return applyCF<false>(column_family_id, key, tick,
                        [&](auto& impl, LocalDocumentId documentId) {
                          impl.recoveryRemove(documentId);
                        });
}

void IResearchRocksDBRecoveryHelper::LogData(rocksdb::Slice const& blob,
                                             rocksdb::SequenceNumber tick) {
  auto const type = RocksDBLogValue::type(blob);

  switch (type) {
    case RocksDBLogType::IndexCreate:
    case RocksDBLogType::IndexDrop: {
      // TODO(MBkkt) More granular cache invalidation
      clear<true>();
    } break;
    case RocksDBLogType::CollectionTruncate: {
      // TODO(MBkkt) Truncate could recover index from OutOfSync state
      auto const objectId = RocksDBLogValue::objectId(blob);

      auto ranges = getRanges(objectId);
      if (ranges.empty()) {
        return;
      }

      auto apply = [&](auto range, auto& values) {
        if (range.empty()) {
          return;
        }
        auto begin = values.begin();
        auto it = begin + range.begin;
        auto end = range.end != kMaxSize ? begin + range.end : values.end();
        for (; it != end; ++it) {
          if (*it == nullptr) {
            continue;
          }
          if (tick <= (**it).recoveryTickLow()) {
            continue;
          }
          if (!skip(**it)) {
            (**it).afterTruncate(tick, nullptr);
          } else {
            (**it).setOutOfSync();
            *it = nullptr;
          }
        }
      };

      apply(ranges.indexes, _indexes);
      apply(ranges.links, _links);
    } break;
    default:
      break;  // shut up the compiler
  }
}

std::shared_ptr<LogicalCollection>
IResearchRocksDBRecoveryHelper::lookupCollection(uint64_t objectId) {
  TRI_ASSERT(_engine);
  TRI_ASSERT(_dbFeature);
  auto const [databaseName, collectionId] =
      _engine->mapObjectToCollection(objectId);
  auto vocbase = _dbFeature->useDatabase(databaseName);
  return vocbase ? vocbase->lookupCollection(collectionId) : nullptr;
}

IResearchRocksDBRecoveryHelper::Ranges&
IResearchRocksDBRecoveryHelper::getRanges(uint64_t objectId) {
  auto [it, emplaced] = _ranges.try_emplace(objectId);
  if (!emplaced) {
    return it->second;
  }
  return it->second = makeRanges(objectId);
}

IResearchRocksDBRecoveryHelper::Ranges
IResearchRocksDBRecoveryHelper::makeRanges(uint64_t objectId) {
  TRI_ASSERT(!_skipAllItems);
  auto collection = lookupCollection(objectId);
  if (!collection) {
    // TODO(MBkkt) it was ok in the old implementation
    // but I don't know why, for me it looks like assert
    return {};
  }

  clear<false>();

  auto const indexesBegin = _indexes.size();
  auto const linksBegin = _links.size();
  TRI_ASSERT(indexesBegin < kMaxSize);
  TRI_ASSERT(linksBegin < kMaxSize);

  auto add = [&](auto& values, auto& value) {
    if (!value.isOutOfSync()) {
      values.push_back(&value);
    }
  };

  for (auto&& index : collection->getIndexes()) {
    TRI_ASSERT(index != nullptr);
    if (index->type() == Index::TRI_IDX_TYPE_INVERTED_INDEX) {
      add(_indexes, basics::downCast<IResearchRocksDBInvertedIndex>(*index));
    } else if (index->type() == Index::TRI_IDX_TYPE_IRESEARCH_LINK) {
      add(_links, basics::downCast<IResearchRocksDBLink>(*index));
    }
  }

  auto const indexesEnd = _indexes.size();
  auto const linksEnd = _links.size();

  return {.indexes = {.begin = static_cast<uint16_t>(indexesBegin),
                      .end = indexesEnd < kMaxSize
                                 ? static_cast<uint16_t>(indexesEnd)
                                 : kMaxSize},
          .links = {.begin = static_cast<uint16_t>(linksBegin),
                    .end = linksEnd < kMaxSize ? static_cast<uint16_t>(linksEnd)
                                               : kMaxSize}};
}

}  // namespace iresearch
}  // namespace arangodb
