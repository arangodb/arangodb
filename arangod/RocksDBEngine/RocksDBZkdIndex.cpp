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

#include "RocksDBZkdIndex.h"

#include <Aql/Variable.h>
#include <Containers/Enumerate.h>
#include <Transaction/Helpers.h>

#include "RocksDBColumnFamilyManager.h"
#include "RocksDBMethods.h"
#include "RocksDBTransactionMethods.h"
#include "Transaction/Methods.h"
#include "Zkd/ZkdHelper.h"

using namespace arangodb;

/*namespace {

auto coordsToVector(zkd::byte_string_view bs, size_t dim) -> std::vector<double>
{ auto vs = zkd::transpose(bs, dim);

  std::vector<double> res;
  res.reserve(dim);

  for (auto&& v : vs) {
    zkd::BitReader r(v);
    std::ignore = r.next();
    res.push_back(zkd::from_bit_reader_fixed_length<double>(r));
  }
  return res;
}

}*/

namespace arangodb {

template<bool isUnique = false>
class RocksDBZkdIndexIterator final : public IndexIterator {
 public:
  RocksDBZkdIndexIterator(LogicalCollection* collection,
                          RocksDBZkdIndexBase* index, transaction::Methods* trx,
                          zkd::byte_string min, zkd::byte_string max,
                          std::size_t dim, ReadOwnWrites readOwnWrites)
      : IndexIterator(collection, trx, readOwnWrites),
        _bound(RocksDBKeyBounds::ZkdIndex(index->objectId())),
        _min(std::move(min)),
        _max(std::move(max)),
        _dim(dim),
        _index(index) {
    _cur = _min;
    _upperBound = _bound.end();

    RocksDBTransactionMethods* mthds =
        RocksDBTransactionState::toMethods(trx, _collection->id());
    _iter = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
      TRI_ASSERT(opts.prefix_same_as_start);
      opts.iterate_upper_bound = &_upperBound;
    });
    TRI_ASSERT(_iter != nullptr);
    _iter->SeekToFirst();
    _compareResult.resize(_dim);
  }

  char const* typeName() const override { return "rocksdb-zkd-index-iterator"; }

 protected:
  bool nextImpl(LocalDocumentIdCallback const& callback,
                size_t limit) override {
    for (auto i = size_t{0}; i < limit;) {
      switch (_iterState) {
        case IterState::SEEK_ITER_TO_CUR: {
          RocksDBKey rocks_key;
          rocks_key.constructZkdIndexValue(_index->objectId(), _cur);
          _iter->Seek(rocks_key.string());
          if (!_iter->Valid()) {
            arangodb::rocksutils::checkIteratorStatus(_iter.get());
            _iterState = IterState::DONE;
          } else {
            TRI_ASSERT(_index->objectId() ==
                       RocksDBKey::objectId(_iter->key()));
            _iterState = IterState::CHECK_CURRENT_ITER;
          }
        } break;
        case IterState::CHECK_CURRENT_ITER: {
          auto const rocksKey = _iter->key();
          auto const byteStringKey = RocksDBKey::zkdIndexValue(rocksKey);
          if (!zkd::testInBox(byteStringKey, _min, _max, _dim)) {
            _cur = byteStringKey;

            zkd::compareWithBoxInto(_cur, _min, _max, _dim, _compareResult);
            auto const next =
                zkd::getNextZValue(_cur, _min, _max, _compareResult);
            if (!next) {
              _iterState = IterState::DONE;
            } else {
              _cur = next.value();
              _iterState = IterState::SEEK_ITER_TO_CUR;
            }
          } else {
            auto const documentId = std::invoke([&] {
              if constexpr (isUnique) {
                return RocksDBValue::documentId(_iter->value());
              } else {
                return RocksDBKey::indexDocumentId(rocksKey);
              }
            });
            std::ignore = callback(documentId);
            ++i;
            _iter->Next();
            if (!_iter->Valid()) {
              arangodb::rocksutils::checkIteratorStatus(_iter.get());
              _iterState = IterState::DONE;
            } else {
              // stay in ::CHECK_CURRENT_ITER
            }
          }
        } break;
        case IterState::DONE:
          return false;
        default:
          TRI_ASSERT(false);
      }
    }

    return true;
  }

 private:
  RocksDBKeyBounds _bound;
  rocksdb::Slice _upperBound;
  zkd::byte_string _cur;
  const zkd::byte_string _min;
  const zkd::byte_string _max;
  const std::size_t _dim;

  enum class IterState {
    SEEK_ITER_TO_CUR = 0,
    CHECK_CURRENT_ITER,
    DONE,
  };
  IterState _iterState = IterState::SEEK_ITER_TO_CUR;

  std::unique_ptr<rocksdb::Iterator> _iter;
  RocksDBZkdIndexBase* _index = nullptr;

  std::vector<zkd::CompareResult> _compareResult;
};

}  // namespace arangodb

namespace {

auto convertDouble(double x) -> zkd::byte_string {
  zkd::BitWriter bw;
  bw.append(zkd::Bit::ZERO);  // add zero bit for `not infinity`
  zkd::into_bit_writer_fixed_length(bw, x);
  return std::move(bw).str();
}

auto nodeExtractDouble(aql::AstNode const* node)
    -> std::optional<zkd::byte_string> {
  if (node != nullptr) {
    return convertDouble(node->getDoubleValue());
  }
  return std::nullopt;
}

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

auto readDocumentKey(
    VPackSlice doc,
    std::vector<std::vector<basics::AttributeName>> const& fields)
    -> zkd::byte_string {
  std::vector<zkd::byte_string> v;
  v.reserve(fields.size());

  for (auto const& path : fields) {
    VPackSlice value = accessDocumentPath(doc, path);
    if (!value.isNumber<double>()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    }
    auto dv = value.getNumericValue<double>();
    if (std::isnan(dv)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE,
                                     "NaN is not allowed");
    }
    v.emplace_back(convertDouble(dv));
  }

  return zkd::interleave(v);
}

auto boundsForIterator(arangodb::Index const* index,
                       const arangodb::aql::AstNode* node,
                       const arangodb::aql::Variable* reference,
                       const arangodb::IndexIteratorOptions& opts)
    -> std::pair<zkd::byte_string, zkd::byte_string> {
  TRI_ASSERT(node->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

  std::unordered_map<size_t, zkd::ExpressionBounds> extractedBounds;
  std::unordered_set<aql::AstNode const*> unusedExpressions;
  extractBoundsFromCondition(index, node, reference, extractedBounds,
                             unusedExpressions);

  TRI_ASSERT(unusedExpressions.empty());

  const size_t dim = index->fields().size();
  std::vector<zkd::byte_string> min;
  min.resize(dim);
  std::vector<zkd::byte_string> max;
  max.resize(dim);

  static const auto ByteStringPosInfinity = zkd::byte_string{std::byte{0x80}};
  static const auto ByteStringNegInfinity = zkd::byte_string{std::byte{0}};

  for (auto&& [idx, field] : enumerate(index->fields())) {
    if (auto it = extractedBounds.find(idx); it != extractedBounds.end()) {
      auto const& bounds = it->second;
      min[idx] = nodeExtractDouble(bounds.lower.bound_value)
                     .value_or(ByteStringNegInfinity);
      max[idx] = nodeExtractDouble(bounds.upper.bound_value)
                     .value_or(ByteStringPosInfinity);
    } else {
      min[idx] = ByteStringNegInfinity;
      max[idx] = ByteStringPosInfinity;
    }
  }

  TRI_ASSERT(min.size() == dim);
  TRI_ASSERT(max.size() == dim);

  return std::make_pair(zkd::interleave(min), zkd::interleave(max));
}
}  // namespace

void zkd::extractBoundsFromCondition(
    arangodb::Index const* index, const arangodb::aql::AstNode* condition,
    const arangodb::aql::Variable* reference,
    std::unordered_map<size_t, ExpressionBounds>& extractedBounds,
    std::unordered_set<aql::AstNode const*>& unusedExpressions) {
  TRI_ASSERT(condition->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

  auto const ensureBounds = [&](size_t idx) -> ExpressionBounds& {
    if (auto it = extractedBounds.find(idx); it != std::end(extractedBounds)) {
      return it->second;
    }
    return extractedBounds[idx];
  };

  auto const useAsBound =
      [&](size_t idx, aql::AstNode* op_node, aql::AstNode* bounded_expr,
          aql::AstNode* bound_value, bool asLower, bool isStrict) {
        auto& bounds = ensureBounds(idx);
        if (asLower) {
          bounds.lower.op_node = op_node;
          bounds.lower.bound_value = bound_value;
          bounds.lower.bounded_expr = bounded_expr;
          bounds.lower.isStrict = isStrict;
        } else {
          bounds.upper.op_node = op_node;
          bounds.upper.bound_value = bound_value;
          bounds.upper.bounded_expr = bounded_expr;
          bounds.upper.isStrict = isStrict;
        }
      };

  auto const checkIsBoundForAttribute =
      [&](aql::AstNode* op, aql::AstNode* access, aql::AstNode* other,
          bool reverse) -> bool {
    std::unordered_set<std::string>
        nonNullAttributes;  // TODO only used in sparse case
    if (!index->canUseConditionPart(access, other, op, reference,
                                    nonNullAttributes, false)) {
      return false;
    }

    std::pair<arangodb::aql::Variable const*,
              std::vector<arangodb::basics::AttributeName>>
        attributeData;
    if (!access->isAttributeAccessForVariable(attributeData) ||
        attributeData.first != reference) {
      // this access is not referencing this collection
      return false;
    }

    for (auto&& [idx, field] : enumerate(index->fields())) {
      if (attributeData.second != field) {
        continue;
      }

      switch (op->type) {
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
          useAsBound(idx, op, access, other, true, false);
          useAsBound(idx, op, access, other, false, false);
          return true;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
          useAsBound(idx, op, access, other, reverse, false);
          return true;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
          useAsBound(idx, op, access, other, !reverse, false);
          return true;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
          useAsBound(idx, op, access, other, reverse, true);
          return true;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
          useAsBound(idx, op, access, other, !reverse, true);
          return true;
        default:
          break;
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
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
        ok |= checkIsBoundForAttribute(op, op->getMember(0), op->getMember(1),
                                       false);
        ok |= checkIsBoundForAttribute(op, op->getMember(1), op->getMember(0),
                                       true);
        break;
      default:
        break;
    }
    if (!ok) {
      unusedExpressions.emplace(op);
    }
  }
}

auto zkd::supportsFilterCondition(
    arangodb::Index const* index,
    const std::vector<std::shared_ptr<arangodb::Index>>& allIndexes,
    const arangodb::aql::AstNode* node,
    const arangodb::aql::Variable* reference, size_t itemsInIndex)
    -> Index::FilterCosts {
  TRI_ASSERT(node->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

  std::unordered_map<size_t, ExpressionBounds> extractedBounds;
  std::unordered_set<aql::AstNode const*> unusedExpressions;
  extractBoundsFromCondition(index, node, reference, extractedBounds,
                             unusedExpressions);

  if (extractedBounds.empty()) {
    return {};
  }

  // TODO -- actually return costs
  auto costs =
      Index::FilterCosts::defaultCosts(itemsInIndex / extractedBounds.size());
  costs.coveredAttributes = extractedBounds.size();
  costs.supportsCondition = true;
  return costs;
}

auto zkd::specializeCondition(arangodb::Index const* index,
                              arangodb::aql::AstNode* condition,
                              const arangodb::aql::Variable* reference)
    -> aql::AstNode* {
  std::unordered_map<size_t, ExpressionBounds> extractedBounds;
  std::unordered_set<aql::AstNode const*> unusedExpressions;
  extractBoundsFromCondition(index, condition, reference, extractedBounds,
                             unusedExpressions);

  std::vector<arangodb::aql::AstNode const*> children;

  for (size_t i = 0; i < condition->numMembers(); ++i) {
    auto op = condition->getMemberUnchecked(i);

    if (unusedExpressions.find(op) == unusedExpressions.end()) {
      switch (op->type) {
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
          children.emplace_back(op);
          break;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
          op->type = aql::NODE_TYPE_OPERATOR_BINARY_LE;
          children.emplace_back(op);
          break;
        case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
          op->type = aql::NODE_TYPE_OPERATOR_BINARY_GE;
          children.emplace_back(op);
          break;
        default:
          break;
      }
    }
  }

  // must edit in place, no access to AST; TODO change so we can replace with
  // copy
  TEMPORARILY_UNLOCK_NODE(condition);
  condition->clearMembers();

  for (auto& it : children) {
    TRI_ASSERT(it->type != arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE);
    condition->addMember(it);
  }

  return condition;
}

arangodb::Result arangodb::RocksDBZkdIndexBase::insert(
    arangodb::transaction::Methods& trx, arangodb::RocksDBMethods* methods,
    const arangodb::LocalDocumentId& documentId,
    arangodb::velocypack::Slice doc, const arangodb::OperationOptions& options,
    bool performChecks) {
  TRI_ASSERT(_unique == false);
  TRI_ASSERT(_sparse == false);

  // TODO what about performChecks?

  auto key_value = readDocumentKey(doc, _fields);

  RocksDBKey rocks_key;
  rocks_key.constructZkdIndexValue(objectId(), key_value, documentId);

  auto value = RocksDBValue::ZkdIndexValue();
  auto s = methods->PutUntracked(_cf, rocks_key, value.string());
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}

arangodb::Result arangodb::RocksDBZkdIndexBase::remove(
    arangodb::transaction::Methods& trx, arangodb::RocksDBMethods* methods,
    const arangodb::LocalDocumentId& documentId,
    arangodb::velocypack::Slice doc) {
  TRI_ASSERT(_unique == false);
  TRI_ASSERT(_sparse == false);

  auto key_value = readDocumentKey(doc, _fields);

  RocksDBKey rocks_key;
  rocks_key.constructZkdIndexValue(objectId(), key_value, documentId);

  auto s = methods->SingleDelete(_cf, rocks_key);
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}

arangodb::RocksDBZkdIndexBase::RocksDBZkdIndexBase(
    arangodb::IndexId iid, arangodb::LogicalCollection& coll,
    arangodb::velocypack::Slice const& info)
    : RocksDBIndex(iid, coll, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::ZkdIndex),
                   false) {}

void arangodb::RocksDBZkdIndexBase::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<arangodb::Index::Serialize>::type type) const {
  VPackObjectBuilder ob(&builder);
  RocksDBIndex::toVelocyPack(builder, type);
}

arangodb::Index::FilterCosts
arangodb::RocksDBZkdIndexBase::supportsFilterCondition(
    const std::vector<std::shared_ptr<arangodb::Index>>& allIndexes,
    const arangodb::aql::AstNode* node,
    const arangodb::aql::Variable* reference, size_t itemsInIndex) const {
  return zkd::supportsFilterCondition(this, allIndexes, node, reference,
                                      itemsInIndex);
}

arangodb::aql::AstNode* arangodb::RocksDBZkdIndexBase::specializeCondition(
    arangodb::aql::AstNode* condition,
    const arangodb::aql::Variable* reference) const {
  return zkd::specializeCondition(this, condition, reference);
}

std::unique_ptr<IndexIterator>
arangodb::RocksDBZkdIndexBase::iteratorForCondition(
    arangodb::transaction::Methods* trx, const arangodb::aql::AstNode* node,
    const arangodb::aql::Variable* reference,
    const arangodb::IndexIteratorOptions& opts, ReadOwnWrites readOwnWrites) {
  auto&& [min, max] = boundsForIterator(this, node, reference, opts);

  return std::make_unique<RocksDBZkdIndexIterator<false>>(
      &_collection, this, trx, std::move(min), std::move(max), fields().size(),
      readOwnWrites);
}

std::unique_ptr<IndexIterator>
arangodb::RocksDBUniqueZkdIndex::iteratorForCondition(
    arangodb::transaction::Methods* trx, const arangodb::aql::AstNode* node,
    const arangodb::aql::Variable* reference,
    const arangodb::IndexIteratorOptions& opts, ReadOwnWrites readOwnWrites) {
  auto&& [min, max] = boundsForIterator(this, node, reference, opts);

  return std::make_unique<RocksDBZkdIndexIterator<true>>(
      &_collection, this, trx, std::move(min), std::move(max), fields().size(),
      readOwnWrites);
}

arangodb::Result arangodb::RocksDBUniqueZkdIndex::insert(
    arangodb::transaction::Methods& trx, arangodb::RocksDBMethods* methods,
    const arangodb::LocalDocumentId& documentId,
    arangodb::velocypack::Slice doc, const arangodb::OperationOptions& options,
    bool performChecks) {
  TRI_ASSERT(_unique == true);
  TRI_ASSERT(_sparse == false);

  // TODO what about performChecks
  auto key_value = readDocumentKey(doc, _fields);

  RocksDBKey rocks_key;
  rocks_key.constructZkdIndexValue(objectId(), key_value);

  if (!options.checkUniqueConstraintsInPreflight) {
    transaction::StringLeaser leased(&trx);
    rocksdb::PinnableSlice existing(leased.get());
    if (auto s = methods->GetForUpdate(_cf, rocks_key.string(), &existing);
        s.ok()) {  // detected conflicting index entry
      return {TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED};
    } else if (!s.IsNotFound()) {
      return Result(rocksutils::convertStatus(s));
    }
  }

  auto value = RocksDBValue::UniqueZkdIndexValue(documentId);

  if (auto s = methods->PutUntracked(_cf, rocks_key, value.string()); !s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}

arangodb::Result arangodb::RocksDBUniqueZkdIndex::remove(
    arangodb::transaction::Methods& trx, arangodb::RocksDBMethods* methods,
    const arangodb::LocalDocumentId& documentId,
    arangodb::velocypack::Slice doc) {
  TRI_ASSERT(_unique == true);
  TRI_ASSERT(_sparse == false);

  auto key_value = readDocumentKey(doc, _fields);

  RocksDBKey rocks_key;
  rocks_key.constructZkdIndexValue(objectId(), key_value);

  auto s = methods->SingleDelete(_cf, rocks_key);
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}
