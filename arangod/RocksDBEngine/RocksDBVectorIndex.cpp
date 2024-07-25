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
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBVectorIndex.h"
#include <cstdint>
#include <cstring>
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Basics/voc-errors.h"
#include "Inspection/VPack.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBIndex.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "Transaction/Helpers.h"
#include "Zkd/ZkdHelper.h"
#include "Utils/ByteString.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/Vector/LocalSensitiveHashing.h"
#endif

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {

class RocksDBVectorIndexIterator final : public IndexIterator {
 public:
  RocksDBVectorIndexIterator(ResourceMonitor& monitor,
                             LogicalCollection* collection,
                             RocksDBVectorIndex* index,
                             transaction::Methods* trx,
                             std::vector<double>&& input,
                             ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _bound(RocksDBKeyBounds::VectorVPackIndex(index->objectId())),
        _index(index) {
    _hashedStrings =
        vector::calculateHashedStrings(_index->getDefinition(), input);
    _itHashedStrings = _hashedStrings.begin();

    _upperBound = _bound.end();
    RocksDBTransactionMethods* mthds =
        RocksDBTransactionState::toMethods(trx, _collection->id());
    _iter = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
      TRI_ASSERT(opts.prefix_same_as_start);
      opts.iterate_upper_bound = &_upperBound;
    });
    TRI_ASSERT(_iter != nullptr);

    _rocksdbKey.constructVectorIndexValue(_index->objectId(),
                                          *_itHashedStrings);
    _iter->Seek(_rocksdbKey.string());
  }

  std::string_view typeName() const noexcept final {
    return "rocksdb-vector-index-iterator";
  }

 protected:
  bool nextImpl(LocalDocumentIdCallback const& callback,
                uint64_t limit) override {
    for (uint64_t i = 0; i < limit;) {
      if (!_iter->Valid()) {
        return false;
      }
      auto key = _iter->key();
      auto indexValue = RocksDBKey::vectorVPackIndexValue(key);

      if (indexValue == *_itHashedStrings) {
        auto documentId = RocksDBKey::indexDocumentId(key);

        if (!_seenDocumentIds.contains(documentId)) {
          _seenDocumentIds.insert(documentId);
          callback(documentId);
          ++i;
        }
        _iter->Next();
      } else {
        ++_itHashedStrings;
        if (_itHashedStrings == _hashedStrings.end()) {
          return false;
        }

        if (indexValue != *_itHashedStrings) {
          _rocksdbKey.constructVectorIndexValue(_index->objectId(),
                                                *_itHashedStrings);
          _iter->Seek(_rocksdbKey.string());
        }
      }
    }

    return true;
  }

  void resetImpl() override {
    _itHashedStrings = _hashedStrings.begin();
    _rocksdbKey.constructVectorIndexValue(_index->objectId(),
                                          *_itHashedStrings);
    _iter->Seek(_rocksdbKey.string());
  }

  bool nextCoveringImpl(CoveringCallback const& callback,
                        uint64_t limit) override {
    return false;
  }

 private:
  RocksDBKey _rocksdbKey;
  rocksdb::Slice _upperBound;
  RocksDBKey _upperBoundKey;
  std::vector<std::uint8_t> _cur;
  RocksDBKeyBounds _bound;

  std::unique_ptr<rocksdb::Iterator> _iter;
  RocksDBVectorIndex* _index = nullptr;
  std::vector<byte_string> _hashedStrings;
  std::vector<byte_string>::iterator _itHashedStrings;
  std::unordered_set<LocalDocumentId> _seenDocumentIds;
};

RocksDBVectorIndex::RocksDBVectorIndex(IndexId iid, LogicalCollection& coll,
                                       arangodb::velocypack::Slice info)
    : RocksDBIndex(iid, coll, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::VectorIndex),
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   coll.vocbase().engine<RocksDBEngine>()) {
  TRI_ASSERT(type() == Index::TRI_IDX_TYPE_VECTOR_INDEX);
  velocypack::deserialize(info.get("params"), _definition);
}

/// @brief Test if this index matches the definition
bool RocksDBVectorIndex::matchesDefinition(VPackSlice const& info) const {
  return false;
}

void RocksDBVectorIndex::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  VPackObjectBuilder objectBuilder(&builder);
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.add(VPackValue("params"));
  velocypack::serialize(builder, _definition);
}

// TODO Move away, also remove from MDIIndex.cpp
auto accessDocumentPath(VPackSlice doc,
                        std::vector<basics::AttributeName> const& path)
    -> VPackSlice {
  for (auto&& attrib : path) {
    TRI_ASSERT(attrib.shouldExpand == false);
    if (!doc.isObject()) {
      return VPackSlice::noneSlice();
    }

    doc = doc.get(attrib.name);
  }

  return doc;
}

template<typename F>
Result RocksDBVectorIndex::processDocument(velocypack::Slice doc,
                                           LocalDocumentId documentId, F func) {
  TRI_ASSERT(_fields.size() == 1);
  VPackSlice value = accessDocumentPath(doc, _fields[0]);
  std::vector<double> input;
  input.reserve(_definition.dimensions);
  if (auto res = velocypack::deserializeWithStatus(value, input); !res.ok()) {
    return {TRI_ERROR_BAD_PARAMETER, res.error()};
  }

  if (input.size() != _definition.dimensions) {
    // TODO Find better error code
    return {TRI_ERROR_BAD_PARAMETER, "sasa"};
  }
  // TODO Maybe check all values withing <min, max>
  auto hashes = vector::calculateHashedStrings(_definition, input);
  // prefix + hash + documentId
  for (auto const& hashedString : hashes) {
    RocksDBKey rocksdbKey;
    rocksdbKey.constructVectorIndexValue(objectId(), hashedString, documentId);

    auto status = func(rocksdbKey);
    if (!status.ok()) {
      return rocksutils::convertStatus(status);
    }
  }

  return Result{};
}

/// @brief inserts a document into the index
Result RocksDBVectorIndex::insert(transaction::Methods& trx,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& options,
                                  bool performChecks) {
#ifdef USE_ENTERPRISE
  return processDocument(doc, documentId, [this, &methods](auto const& key) {
    auto value = RocksDBValue::VectorIndexValue();
    return methods->PutUntracked(this->_cf, key, value.string());
  });
#else
  return Result(TRI_ERROR_ONLY_ENTERPRISE);
#endif
}

Index::FilterCosts RocksDBVectorIndex::supportsFilterCondition(
    transaction::Methods& /*trx*/,
    std::vector<std::shared_ptr<Index>> const& allIndexes,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) const {
  TRI_ASSERT(node->numMembers() == 1);
  auto const* firstMemeber = node->getMember(0);
  if (firstMemeber->type == aql::NODE_TYPE_FCALL) {
    auto const* funcNode =
        static_cast<aql::Function const*>(firstMemeber->getData());

    if (funcNode->name == "APPROX_NEAR") {
      return {.supportsCondition = true};
    }
  }
  return {};
}

/// @brief removes a document from the index
Result RocksDBVectorIndex::remove(transaction::Methods& trx,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& options) {
#ifdef USE_ENTERPRISE
  return processDocument(doc, documentId, [this, &methods](auto const& key) {
    return methods->Delete(this->_cf, key);
  });
#else
  return Result(TRI_ERROR_ONLY_ENTERPRISE);
#endif
}

std::unique_ptr<IndexIterator> RocksDBVectorIndex::iteratorForCondition(
    ResourceMonitor& monitor, transaction::Methods* trx,
    aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites, int) {
  node = node->getMember(0);
  TRI_ASSERT(node->type == aql::NODE_TYPE_FCALL);
  // auto const* funcNode = static_cast<aql::Function const*>(node->getData());
  auto const* funcNode = static_cast<aql::Function const*>(node->getData());

  TRI_ASSERT(funcNode->name == "APPROX_NEAR");

  auto const* functionCallParams = node->getMember(0);
  TRI_ASSERT(functionCallParams->type == aql::NODE_TYPE_ARRAY);
  TRI_ASSERT(functionCallParams->numMembers() == 2);

  auto const* rhs = functionCallParams->getMember(1);
  TRI_ASSERT(rhs->type == aql::NODE_TYPE_ARRAY);

  std::vector<double> input;
  input.reserve(rhs->numMembers());
  for (size_t i = 0; i < rhs->numMembers(); ++i) {
    input.push_back(rhs->getMember(i)->getDoubleValue());
  }

  return std::make_unique<RocksDBVectorIndexIterator>(
      monitor, &_collection, this, trx, std::move(input), readOwnWrites);
}

// Remove conditions covered by this index
aql::AstNode* RocksDBVectorIndex::specializeCondition(
    transaction::Methods& trx, aql::AstNode* condition,
    aql::Variable const* reference) const {
  if (!condition->hasFlag(aql::AstNodeFlagType::FLAG_FINALIZED)) {
    condition->clearMembers();
  }
  return condition;
}

}  // namespace arangodb
