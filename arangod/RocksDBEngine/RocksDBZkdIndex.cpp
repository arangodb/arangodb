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
        _dim(dim) {
    _cur = _min;
    _upperBound = _bound.end();

    RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);
    rocksdb::ReadOptions options = mthds->iteratorReadOptions();
    options.iterate_upper_bound = &_upperBound;
    TRI_ASSERT(options.prefix_same_as_start);
    _iter = mthds->NewIterator(options, index->columnFamily());
    TRI_ASSERT(_iter != nullptr);
  }


  char const* typeName() const override { return "rocksdb-zkd-index-iterator"; }

 protected:
  bool nextImpl(const LocalDocumentIdCallback& callback, size_t limit) override {


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

}

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

  std::unordered_set<size_t> bounded_below;
  std::unordered_set<size_t> bounded_above;

  auto const checkIsBoundForAttribute = [&](aql::AstNode* op, aql::AstNode* access,
                                            aql::AstNode* other) -> bool {
    LOG_DEVEL << "checkIsBoundForAttribute op " << op->toString();
    std::unordered_set<std::string> nonNullAttributes;  // TODO only used in sparse case
    if (!canUseConditionPart(access, other, op, reference, nonNullAttributes, false)) {
      LOG_DEVEL << "!canUseConditionPart";
      return false;
    }

    std::pair<arangodb::aql::Variable const*, std::vector<arangodb::basics::AttributeName>> attributeData;
    if (!access->isAttributeAccessForVariable(attributeData) ||
        attributeData.first != reference) {
      LOG_DEVEL << "!isAttributeAccessForVariable";
      // this access is not referencing this collection
      return false;
    }

    auto& path = attributeData.second;
    // TODO -- make this more generic
    if (path.size() != 1 || path[0].shouldExpand) {
      LOG_DEVEL << "bad path config";
      return false;
    }

    auto& name = path[0].name;
    for (size_t i = 0; i < _fields.size(); i++) {
      auto const& field = _fields[i];
      LOG_DEVEL << "name = " << name << " field = " << field[0].name;
      TRI_ASSERT(field.size() == 1);

      if (name == field[0].name) {
        switch (op->type) {
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
            // TODO
            TRI_ASSERT(false);
          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
            if (bounded_below.find(i) != bounded_below.end()) {
              return false;
            }
            bounded_below.emplace(i);
            LOG_DEVEL << "new bound below!";
            return true;

          case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
            if (bounded_above.find(i) != bounded_above.end()) {
              return false;
            }
            bounded_above.emplace(i);
            LOG_DEVEL << "new bound above!";
            return true;
          default:
            TRI_ASSERT(false);
        }
      }
    }

    return false;
  };

  for (size_t i = 0; i < node->numMembers(); ++i) {
    bool ok = false;
    auto op = node->getMemberUnchecked(i);
    switch (op->type) {
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
        ok |= checkIsBoundForAttribute(op, op->getMember(0), op->getMember(1));
        ok |= checkIsBoundForAttribute(op, op->getMember(1), op->getMember(0));
        if (ok) {
          break;
        }
        [[fallthrough]];
      default:
        LOG_DEVEL << "Not a valid bound for an attribute";
        return FilterCosts();  // not supported
    }
  }

  // TODO -- trigger unbounded query
  if (bounded_above.size() != bounded_below.size()) {
    LOG_DEVEL << "Not all variables have bounds on both sides";
    return FilterCosts();  // not supported
  }

  if (bounded_above.size() != _fields.size()) {
    LOG_DEVEL << "Not all fields have bounds";
    return FilterCosts();  // not supported
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

  const size_t dim = _fields.size();
  std::vector<zkd::byte_string> min;
  std::vector<zkd::byte_string> max;
  // TODO -- compute bounds

  TRI_ASSERT(min.size() == dim);
  TRI_ASSERT(max.size() == dim);
  return std::make_unique<RocksDBZkdIndexIterator>(&_collection, this, trx,
                                                   zkd::interleave(min),
                                                   zkd::interleave(max), dim);
}
