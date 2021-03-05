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
/// @author Tobias Gödderz
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Zkd/ZkdHelper.h"

#include <Aql/Variable.h>
#include <Containers/Enumerate.h>
#include "RocksDBColumnFamilyManager.h"
#include "RocksDBMethods.h"
#include "RocksDBZkdIndex.h"
#include "Transaction/Methods.h"

using namespace arangodb;

namespace arangodb {
class RocksDBZkdIndexIterator final : public IndexIterator {
 public:
  RocksDBZkdIndexIterator(LogicalCollection* collection, RocksDBZkdIndex* index,
                          transaction::Methods* trx, zkd::byte_string min,
                          zkd::byte_string max, std::size_t dim)
      : IndexIterator(collection, trx),
        _bound(RocksDBKeyBounds::ZkdIndex(index->objectId())),
        _min(std::move(min)),
        _max(std::move(max)),
        _dim(dim),
        _index(index) {
    _cur = _min;
    _upperBound = _bound.end();

    RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);
    rocksdb::ReadOptions options = mthds->iteratorReadOptions();
    options.iterate_upper_bound = &_upperBound;
    TRI_ASSERT(options.prefix_same_as_start);
    _iter = mthds->NewIterator(options, index->columnFamily());
    TRI_ASSERT(_iter != nullptr);
    _iter->SeekToFirst();
  }

  char const* typeName() const override { return "rocksdb-zkd-index-iterator"; }

 protected:
  bool nextImpl(const LocalDocumentIdCallback& callback, size_t limit) override {
    while (true) {
      if (!_iter->Valid()) {
        arangodb::rocksutils::checkIteratorStatus(_iter.get());
        return false;
      }

      TRI_ASSERT(_index->objectId() == RocksDBKey::objectId(_iter->key()));

      bool more = callback(RocksDBKey::documentId(_iter->key()));
      _iter->Next();
      if (!more) {
        break;
      }
    }

    return _iter->Valid();
  }

  /*

  auto iter = std::unique_ptr<rocksdb::Iterator>{rocks->db->NewIterator(rocksdb::ReadOptions{})};

  while (true) {
    //std::cout << "Seeking to " << cur << std::endl;
    iter->Seek(sliceFromString(cur));
    num_seeks += 1;
    auto s = iter->status();
    if (!s.ok()) {
      std::cerr << s.ToString() << std::endl;
      return {};
    }
    if (!iter->Valid()) {
      break;
    }

    while (true) {
      auto key = viewFromSlice(iter->key());
      if (!testInBox(key, min_s, max_s, 4)) {
        cur = key;
        break;
      }

      auto value = transpose(byte_string{key}, 4);

      //std::cout << value[0] << " " << value[1] << " " << value[2] << " " << value[3] << std::endl;
      res.insert({from_byte_string_fixed_length<double>(value[0]),
          from_byte_string_fixed_length<double>(value[1]),
                 from_byte_string_fixed_length<double>(value[2]),
                 from_byte_string_fixed_length<double>(value[3])});
      iter->Next();
    }

    auto cmp = compareWithBox(cur, min_s, max_s, 4);

    auto next = getNextZValue(cur, min_s, max_s, cmp);
    if (!next) {
      break;
    }

    cur = next.value();
  }
   */

  RocksDBKeyBounds _bound;
  rocksdb::Slice _upperBound;
  zkd::byte_string _cur;
  const zkd::byte_string _min;
  const zkd::byte_string _max;
  const std::size_t _dim;

  std::unique_ptr<rocksdb::Iterator> _iter;
  RocksDBZkdIndex* _index;
};

}  // namespace arangodb

namespace {

auto readDocumentKey(VPackSlice doc,
                     std::vector<std::vector<basics::AttributeName>> const& fields)
    -> zkd::byte_string {
  std::vector<zkd::byte_string> v;
  v.reserve(fields.size());

  for (auto const& path : fields) {
    TRI_ASSERT(path.size() == 1);
    TRI_ASSERT(path[0].shouldExpand == false);
    VPackSlice value = doc.get(path[0].name);
    if (!value.isNumber<double>()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    }
    v.emplace_back(zkd::to_byte_string_fixed_length(value.getNumericValue<double>()));
  }

  return zkd::interleave(v);
}

using ExpressionBounds = std::pair<aql::AstNode const*, aql::AstNode const*>;

void extractBoundsFromCondition(
    RocksDBZkdIndex const* index,
    const arangodb::aql::AstNode* condition, const arangodb::aql::Variable* reference,
    std::unordered_map<size_t, ExpressionBounds>& extractedBounds,
    std::unordered_set<aql::AstNode const*>& unusedExpressions) {
  TRI_ASSERT(condition->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

  auto const ensureBounds = [&](size_t idx) -> ExpressionBounds& {
    if (auto it = extractedBounds.find(idx); it != std::end(extractedBounds)) {
      return it->second;
    }
    return extractedBounds[idx] = std::make_pair(nullptr, nullptr);
  };

  auto const useAsLowerBound = [&](size_t idx, aql::AstNode* node) {
    ensureBounds(idx).first = node;
  };
  auto const useAsUpperBound = [&](size_t idx, aql::AstNode* node) {
    ensureBounds(idx).second = node;
  };

  auto const checkIsBoundForAttribute = [&](aql::AstNode* op, aql::AstNode* access,
                                            aql::AstNode* other) -> bool {

    std::unordered_set<std::string> nonNullAttributes;  // TODO only used in sparse case
    if (!index->canUseConditionPart(access, other, op, reference, nonNullAttributes, false)) {
      return false;
    }

    std::pair<arangodb::aql::Variable const*, std::vector<arangodb::basics::AttributeName>> attributeData;
    if (!access->isAttributeAccessForVariable(attributeData) ||
        attributeData.first != reference) {
      // this access is not referencing this collection
      return false;
    }

    auto& path = attributeData.second;
    // TODO -- make this more generic
    TRI_ASSERT(path.size() == 1 && !path[0].shouldExpand);

    auto& name = path[0].name;
    for (auto&& [idx, field] : enumerate(index->fields())) {
      TRI_ASSERT(field.size() == 1);

      if (name != field[0].name) {
        continue;
      }

      if (name == field[0].name) {
        switch (op->type) {
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
            useAsLowerBound(idx, other);
            useAsUpperBound(idx, other);
            return true;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
            useAsLowerBound(idx, other);
            return true;
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
            useAsUpperBound(idx, other);
            return true;
          default:
            break;
        }
      }
    }

    return false;
  };


  for (size_t i = 0; i < condition->numMembers(); ++i) {
    bool ok = false;
    auto op = condition->getMemberUnchecked(i);
    switch (op->type) {
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
        ok |= checkIsBoundForAttribute(op, op->getMember(0), op->getMember(1));
        ok |= checkIsBoundForAttribute(op, op->getMember(1), op->getMember(0));
        break;
      default:
        break;
    }
    if (!ok) {
      unusedExpressions.emplace(op);
    }
  }
}

}  // namespace

arangodb::Result arangodb::RocksDBZkdIndex::insert(
    arangodb::transaction::Methods& trx, arangodb::RocksDBMethods* methods,
    const arangodb::LocalDocumentId& documentId,
    arangodb::velocypack::Slice doc, const arangodb::OperationOptions& options) {
  TRI_ASSERT(_unique == false);
  TRI_ASSERT(_sparse == false);

  auto key_value = readDocumentKey(doc, _fields);

  LOG_DEVEL << "RocksDBZkdIndex::insert documentId = " << documentId.id()
            << " doc = " << doc.toJson() << " key = " << key_value;

  RocksDBKey rocks_key;
  rocks_key.constructZkdIndexValue(objectId(), key_value, documentId);

  auto value = RocksDBValue::ZkdIndexValue();
  auto s = methods->PutUntracked(_cf, rocks_key, value.string());
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return Result();
}

arangodb::Result arangodb::RocksDBZkdIndex::remove(arangodb::transaction::Methods& trx,
                                                   arangodb::RocksDBMethods* methods,
                                                   const arangodb::LocalDocumentId& documentId,
                                                   arangodb::velocypack::Slice doc) {
  TRI_ASSERT(_unique == false);
  TRI_ASSERT(_sparse == false);

  auto key_value = readDocumentKey(doc, _fields) +
                   zkd::to_byte_string_fixed_length(documentId.id());

  LOG_DEVEL << "RocksDBZkdIndex::remove documentId = " << documentId.id()
            << " doc = " << doc.toJson() << " key = " << key_value;

  RocksDBKey rocks_key;
  rocks_key.constructZkdIndexValue(objectId(), key_value, documentId);

  auto value = RocksDBValue::ZkdIndexValue();
  auto s = methods->SingleDelete(_cf, rocks_key);
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return Result();
}

arangodb::RocksDBZkdIndex::RocksDBZkdIndex(arangodb::IndexId iid,
                                           arangodb::LogicalCollection& coll,
                                           const arangodb::velocypack::Slice& info)
    : RocksDBIndex(iid, coll, info,
                   /* TODO maybe we want to add a new column family? However, we can not use VPackIndexes because
                    * they use the vpack comparator. For now use GeoIndex because it uses just the 8 byte prefix.
                    */
                   RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::GeoIndex),
                   false) {}

void arangodb::RocksDBZkdIndex::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<arangodb::Index::Serialize>::type type) const {
  VPackObjectBuilder ob(&builder);
  RocksDBIndex::toVelocyPack(builder, type);
  builder.add("dimension", VPackValue(_fields.size()));
}

arangodb::Index::FilterCosts arangodb::RocksDBZkdIndex::supportsFilterCondition(
    const std::vector<std::shared_ptr<arangodb::Index>>& allIndexes,
    const arangodb::aql::AstNode* node,
    const arangodb::aql::Variable* reference, size_t itemsInIndex) const {
  LOG_DEVEL << "RocksDBZkdIndex::supportsFilterCondition node = " << node->toString()
            << " reference = " << reference->name;

  TRI_ASSERT(node->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

  std::unordered_map<size_t, ExpressionBounds> extractedBounds;
  std::unordered_set<aql::AstNode const*> unusedExpressions;
  extractBoundsFromCondition(this, node, reference, extractedBounds, unusedExpressions);

  if (!unusedExpressions.empty()) {
    return FilterCosts();
  }

  bool allBound =
      std::all_of(extractedBounds.begin(), extractedBounds.end(), [&](auto&& kv) {
        auto&& [idx, bounds] = kv;
        return bounds.first != nullptr && bounds.second != nullptr;
      });
  if (!allBound || extractedBounds.size() != _fields.size()) {
    LOG_DEVEL << "Not all variables are bound";
    return FilterCosts();
  }

  LOG_DEVEL << "We can use this index!";
  // TODO -- actually return costs
  return FilterCosts::zeroCosts();
}
arangodb::aql::AstNode* arangodb::RocksDBZkdIndex::specializeCondition(
    arangodb::aql::AstNode* node, const arangodb::aql::Variable* reference) const {
  // TRI_ASSERT(false);
  // THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  // TODO -- implement something here
  return node;
}

std::unique_ptr<IndexIterator> arangodb::RocksDBZkdIndex::iteratorForCondition(
    arangodb::transaction::Methods* trx, const arangodb::aql::AstNode* node,
    const arangodb::aql::Variable* reference, const arangodb::IndexIteratorOptions& opts) {
  LOG_DEVEL << "RocksDBZkdIndex::iteratorForCondition node = " << node->toString()
            << " ref = " << reference->id;

  TRI_ASSERT(node->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

  std::unordered_map<size_t, ExpressionBounds> extractedBounds;
  std::unordered_set<aql::AstNode const*> unusedExpressions;
  extractBoundsFromCondition(this, node, reference, extractedBounds, unusedExpressions);

  TRI_ASSERT(unusedExpressions.empty());

  bool allBound =
      std::all_of(extractedBounds.begin(), extractedBounds.end(), [&](auto&& kv) {
        auto&& [idx, bounds] = kv;
        return bounds.first != nullptr && bounds.second != nullptr;
      });
  TRI_ASSERT(allBound && extractedBounds.size() == _fields.size());

  const size_t dim = _fields.size();
  std::vector<zkd::byte_string> min;
  min.resize(dim);
  std::vector<zkd::byte_string> max;
  max.resize(dim);

  for (auto&& [idx, bounds] : extractedBounds) {
    min[idx] = zkd::to_byte_string_fixed_length(bounds.first->getDoubleValue());
    max[idx] = zkd::to_byte_string_fixed_length(bounds.second->getDoubleValue());
  }

  TRI_ASSERT(min.size() == dim);
  TRI_ASSERT(max.size() == dim);
  return std::make_unique<RocksDBZkdIndexIterator>(&_collection, this, trx,
                                                   zkd::interleave(min),
                                                   zkd::interleave(max), dim);
}
