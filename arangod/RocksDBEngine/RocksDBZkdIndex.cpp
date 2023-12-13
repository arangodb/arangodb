////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Variable.h"
#include "Containers/Enumerate.h"
#include "Containers/FlatHashSet.h"
#include "Transaction/Helpers.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "Zkd/ZkdHelper.h"
#include "Logger/LogMacros.h"
#include "ClusterEngine/ClusterIndex.h"

using namespace arangodb;

namespace arangodb {

template<bool isUnique = false>
class RocksDBZkdIndexIterator final : public IndexIterator {
 public:
  RocksDBZkdIndexIterator(ResourceMonitor& monitor,
                          LogicalCollection* collection,
                          RocksDBZkdIndexBase* index, transaction::Methods* trx,
                          zkd::byte_string min, zkd::byte_string max,
                          transaction::BuilderLeaser prefix, std::size_t dim,
                          ReadOwnWrites readOwnWrites, size_t lookahead)
      : IndexIterator(collection, trx, readOwnWrites),
        _min(std::move(min)),
        _max(std::move(max)),
        _dim(dim),
        _prefix(std::move(prefix)),
        _index(index),
        _lookahead(lookahead) {
    _cur = _min;

    VPackBuilder builder;
    {
      VPackArrayBuilder ab(&builder);
      builder.add(VPackArrayIterator(_prefix->slice()));
      builder.add(VPackSlice::maxKeySlice());
    }

    _upperBoundKey.constructZkdIndexValue(index->objectId(), builder.slice(),
                                          {});
    _upperBound = _upperBoundKey.string();

    RocksDBTransactionMethods* mthds =
        RocksDBTransactionState::toMethods(trx, _collection->id());
    _iter = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
      TRI_ASSERT(opts.prefix_same_as_start);
      opts.iterate_upper_bound = &_upperBound;
    });
    TRI_ASSERT(_iter != nullptr);
    _compareResult.resize(_dim);
  }

  RocksDBKey _upperBoundKey;

  std::string_view typeName() const noexcept final {
    return "rocksdb-zkd-index-iterator";
  }

 protected:
  size_t numNextTries()
      const noexcept {  // may depend on the number of dimensions
                        // and the limits of the query
    return _lookahead;
  }

  template<typename F>
  bool findNext(F&& callback, uint64_t limit) {
    for (uint64_t i = 0; i < limit;) {
      switch (_iterState) {
        case IterState::SEEK_ITER_TO_CUR: {
          RocksDBKey rocks_key;
          rocks_key.constructZkdIndexValue(_index->objectId(), _prefix->slice(),
                                           _cur);
          _iter->Seek(rocks_key.string());

          if (!_iter->Valid()) {
            rocksutils::checkIteratorStatus(*_iter);
            _iterState = IterState::DONE;
          } else {
            TRI_ASSERT(_index->objectId() ==
                       RocksDBKey::objectId(_iter->key()));
            _iterState = IterState::CHECK_CURRENT_ITER;
          }
        } break;
        case IterState::CHECK_CURRENT_ITER: {
          auto rocksKey = _iter->key();
          auto byteStringKey = RocksDBKey::zkdIndexValue(rocksKey);

          bool foundNextZValueInBox =
              zkd::testInBox(byteStringKey, _min, _max, _dim);
          for (size_t numTried = 0;
               !foundNextZValueInBox && numTried < numNextTries(); ++numTried) {
            _iter->Next();
            if (!_iter->Valid()) {
              rocksutils::checkIteratorStatus(*_iter);
              _iterState = IterState::DONE;
              break;  // for loop
            }
            rocksKey = _iter->key();
            byteStringKey = RocksDBKey::zkdIndexValue(rocksKey);
            foundNextZValueInBox =
                zkd::testInBox(byteStringKey, _min, _max, _dim);
          }

          if (_iterState == IterState::DONE) {
            break;  // case CHECK_CURRENT_ITER
          }

          if (!foundNextZValueInBox) {
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
            callback(rocksKey, _iter->value());
            ++i;
            _iter->Next();
            if (!_iter->Valid()) {
              rocksutils::checkIteratorStatus(*_iter);
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

  bool nextImpl(LocalDocumentIdCallback const& callback,
                uint64_t limit) override {
    return findNext(
        [&](rocksdb::Slice key, rocksdb::Slice value) {
          auto const documentId = std::invoke([&] {
            if constexpr (isUnique) {
              return RocksDBValue::documentId(value);
            } else {
              return RocksDBKey::indexDocumentId(key);
            }
          });
          std::ignore = callback(documentId);
        },
        limit);
  }

  bool nextCoveringImpl(CoveringCallback const& callback,
                        uint64_t limit) override {
    struct CoveringData : IndexIteratorCoveringData {
      explicit CoveringData(const velocypack::Slice& data) : _data(data) {}
      VPackSlice at(size_t i) const override { return _data.at(i); }
      bool isArray() const noexcept override { return true; }
      velocypack::ValueLength length() const override { return _data.length(); }

      velocypack::Slice _data;
    };
    return findNext(
        [&](rocksdb::Slice key, rocksdb::Slice value) {
          auto const documentId = std::invoke([&] {
            if constexpr (isUnique) {
              return RocksDBValue::documentId(value);
            } else {
              return RocksDBKey::indexDocumentId(key);
            }
          });

          auto storedValues = std::invoke([&] {
            if constexpr (isUnique) {
              return RocksDBValue::uniqueIndexStoredValues(value);
            } else {
              return RocksDBValue::indexStoredValues(value);
            }
          });
          CoveringData coveringData{storedValues};
          std::ignore = callback(documentId, coveringData);
        },
        limit);
  }

 private:
  rocksdb::Slice _upperBound;
  zkd::byte_string _cur;
  const zkd::byte_string _min;
  const zkd::byte_string _max;
  const std::size_t _dim;
  transaction::BuilderLeaser const _prefix;

  enum class IterState {
    SEEK_ITER_TO_CUR = 0,
    CHECK_CURRENT_ITER,
    DONE,
  };
  IterState _iterState = IterState::SEEK_ITER_TO_CUR;

  std::unique_ptr<rocksdb::Iterator> _iter;
  RocksDBZkdIndexBase* _index = nullptr;

  const size_t _lookahead;

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

auto boundsForIterator(RocksDBZkdIndexBase const* index,
                       const aql::AstNode* node, const aql::Variable* reference,
                       const IndexIteratorOptions& opts,
                       velocypack::Builder& prefixValuesBuilder)
    -> std::pair<zkd::byte_string, zkd::byte_string> {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
  std::unordered_map<size_t, aql::AstNode const*> extractedPrefix;
  std::unordered_map<size_t, zkd::ExpressionBounds> extractedBounds;
  std::unordered_set<aql::AstNode const*> unusedExpressions;
  extractBoundsFromCondition(index, node, reference, extractedPrefix,
                             extractedBounds, unusedExpressions);

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

  prefixValuesBuilder.clear();
  prefixValuesBuilder.openArray();
  for (auto&& [idx, field] : enumerate(index->sortedPrefixFields())) {
    auto it = extractedPrefix.find(idx);
    TRI_ASSERT(it != extractedPrefix.end());
    aql::AstNode const* value = it->second;
    TRI_ASSERT(value->isConstant());
    value->toVelocyPackValue(prefixValuesBuilder);
  }
  prefixValuesBuilder.close();

  TRI_ASSERT(min.size() == dim);
  TRI_ASSERT(max.size() == dim);

  return std::make_pair(zkd::interleave(min), zkd::interleave(max));
}

std::vector<std::vector<basics::AttributeName>> const& getSortedPrefixFields(
    Index const* index) {
  if (auto ptr = dynamic_cast<RocksDBZkdIndexBase const*>(index);
      ptr != nullptr) {
    return ptr->sortedPrefixFields();
  }
  if (auto ptr = dynamic_cast<ClusterIndex const*>(index); ptr != nullptr) {
    return ptr->sortedPrefixFields();
  }
  return Index::emptyCoveredFields;
}

}  // namespace

void zkd::extractBoundsFromCondition(
    Index const* index, const aql::AstNode* condition,
    const aql::Variable* reference,
    std::unordered_map<size_t, aql::AstNode const*>& extractedPrefix,
    std::unordered_map<size_t, ExpressionBounds>& extractedBounds,
    std::unordered_set<aql::AstNode const*>& unusedExpressions) {
  TRI_ASSERT(condition->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

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
    containers::FlatHashSet<std::string>
        nonNullAttributes;  // TODO only used in sparse case
    if (!index->canUseConditionPart(access, other, op, reference,
                                    nonNullAttributes, false)) {
      return false;
    }

    std::pair<aql::Variable const*, std::vector<basics::AttributeName>>
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
        case aql::NODE_TYPE_OPERATOR_BINARY_EQ:
          useAsBound(idx, op, access, other, true, false);
          useAsBound(idx, op, access, other, false, false);
          return true;
        case aql::NODE_TYPE_OPERATOR_BINARY_LE:
          useAsBound(idx, op, access, other, reverse, false);
          return true;
        case aql::NODE_TYPE_OPERATOR_BINARY_GE:
          useAsBound(idx, op, access, other, !reverse, false);
          return true;
        case aql::NODE_TYPE_OPERATOR_BINARY_LT:
          useAsBound(idx, op, access, other, reverse, true);
          return true;
        case aql::NODE_TYPE_OPERATOR_BINARY_GT:
          useAsBound(idx, op, access, other, !reverse, true);
          return true;
        default:
          break;
      }
    }

    return false;
  };

  auto const checkIsPrefixValue = [&](aql::AstNode* op, aql::AstNode* access,
                                      aql::AstNode* other) -> bool {
    TRI_ASSERT(op->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ);

    std::pair<aql::Variable const*, std::vector<basics::AttributeName>>
        attributeData;
    if (!access->isAttributeAccessForVariable(attributeData) ||
        attributeData.first != reference) {
      // this access is not referencing this collection
      return false;
    }

    for (auto&& [idx, field] : enumerate(getSortedPrefixFields(index))) {
      if (attributeData.second != field) {
        continue;
      }

      auto [it, inserted] = extractedPrefix.emplace(idx, other);
      TRI_ASSERT(inserted) << "duplicate access for " << attributeData.second;
      return true;
    }
    return false;
  };

  for (size_t i = 0; i < condition->numMembers(); ++i) {
    bool ok = false;
    auto op = condition->getMemberUnchecked(i);
    switch (op->type) {
      case aql::NODE_TYPE_OPERATOR_BINARY_EQ:
        ok |= checkIsPrefixValue(op, op->getMember(0), op->getMember(1));
        ok |= checkIsPrefixValue(op, op->getMember(1), op->getMember(0));
        if (ok) {
          break;
        }
        [[fallthrough]];
      case aql::NODE_TYPE_OPERATOR_BINARY_LE:
      case aql::NODE_TYPE_OPERATOR_BINARY_GE:
      case aql::NODE_TYPE_OPERATOR_BINARY_LT:
      case aql::NODE_TYPE_OPERATOR_BINARY_GT:
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
    Index const* index, const std::vector<std::shared_ptr<Index>>& allIndexes,
    const aql::AstNode* node, const aql::Variable* reference,
    size_t itemsInIndex) -> Index::FilterCosts {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
  std::unordered_map<size_t, aql::AstNode const*> extractedPrefix;
  std::unordered_map<size_t, ExpressionBounds> extractedBounds;
  std::unordered_set<aql::AstNode const*> unusedExpressions;
  extractBoundsFromCondition(index, node, reference, extractedPrefix,
                             extractedBounds, unusedExpressions);

  if (extractedBounds.empty()) {
    return {};
  }

  if (extractedPrefix.size() != getSortedPrefixFields(index).size()) {
    return {};  // all prefix values have to be assigned
  }

  // TODO -- actually return costs
  auto costs = Index::FilterCosts::defaultCosts(
      /*itemsInIndex / extractedBounds.size()*/ 1);
  costs.coveredAttributes = extractedBounds.size();
  costs.supportsCondition = true;
  return costs;
}

auto zkd::specializeCondition(Index const* index, aql::AstNode* condition,
                              const aql::Variable* reference) -> aql::AstNode* {
  std::unordered_map<size_t, aql::AstNode const*> extractedPrefix;
  std::unordered_map<size_t, ExpressionBounds> extractedBounds;
  std::unordered_set<aql::AstNode const*> unusedExpressions;
  extractBoundsFromCondition(index, condition, reference, extractedPrefix,
                             extractedBounds, unusedExpressions);

  std::vector<aql::AstNode const*> children;

  for (size_t i = 0; i < condition->numMembers(); ++i) {
    auto op = condition->getMemberUnchecked(i);

    if (unusedExpressions.find(op) == unusedExpressions.end()) {
      switch (op->type) {
        case aql::NODE_TYPE_OPERATOR_BINARY_EQ:
        case aql::NODE_TYPE_OPERATOR_BINARY_LE:
        case aql::NODE_TYPE_OPERATOR_BINARY_GE:
          children.emplace_back(op);
          break;
        case aql::NODE_TYPE_OPERATOR_BINARY_LT:
          op->type = aql::NODE_TYPE_OPERATOR_BINARY_LE;
          children.emplace_back(op);
          break;
        case aql::NODE_TYPE_OPERATOR_BINARY_GT:
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
    TRI_ASSERT(it->type != aql::NODE_TYPE_OPERATOR_BINARY_NE);
    condition->addMember(it);
  }

  return condition;
}

namespace {
auto extractAttributeValues(
    transaction::Methods& trx,
    std::vector<std::vector<basics::AttributeName>> const& storedValues,
    velocypack::Slice doc) {
  transaction::BuilderLeaser leased(&trx);
  leased->openArray(true);
  for (auto const& it : storedValues) {
    VPackSlice s;
    if (it.size() == 1 && it[0].name == StaticStrings::IdString) {
      // instead of storing the value of _id, we instead store the
      // value of _key. we will retranslate the value to an _id later
      // again upon retrieval
      s = transaction::helpers::extractKeyFromDocument(doc);
    } else {
      s = doc;
      for (auto const& part : it) {
        s = s.get(part.name);
        if (s.isNone()) {
          break;
        }
      }
    }
    if (s.isNone()) {
      s = VPackSlice::nullSlice();
    }
    leased->add(s);
  }
  leased->close();

  return leased;
}
}  // namespace

Result RocksDBZkdIndexBase::insert(transaction::Methods& trx,
                                   RocksDBMethods* methods,
                                   LocalDocumentId documentId,
                                   velocypack::Slice doc,
                                   OperationOptions const& options,
                                   bool performChecks) {
  TRI_ASSERT(_unique == false);
  TRI_ASSERT(_sparse == false);

  auto key_value = readDocumentKey(doc, _fields);
  auto prefixValues = extractAttributeValues(trx, _sortedPrefixValues, doc);

  RocksDBKey rocks_key;
  rocks_key.constructZkdIndexValue(objectId(), prefixValues->slice(), key_value,
                                   documentId);

  auto storedValues = extractAttributeValues(trx, _storedValues, doc);
  auto value = RocksDBValue::ZkdIndexValue(storedValues->slice());
  auto s = methods->PutUntracked(_cf, rocks_key, value.string());
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}

Result RocksDBZkdIndexBase::remove(transaction::Methods& trx,
                                   RocksDBMethods* methods,
                                   LocalDocumentId documentId,
                                   velocypack::Slice doc,
                                   OperationOptions const& /*options*/) {
  TRI_ASSERT(_unique == false);
  TRI_ASSERT(_sparse == false);

  auto key_value = readDocumentKey(doc, _fields);
  auto prefixValues = extractAttributeValues(trx, _sortedPrefixValues, doc);

  RocksDBKey rocks_key;
  rocks_key.constructZkdIndexValue(objectId(), prefixValues->slice(), key_value,
                                   documentId);

  auto s = methods->SingleDelete(_cf, rocks_key);
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}

RocksDBZkdIndexBase::RocksDBZkdIndexBase(IndexId iid, LogicalCollection& coll,
                                         velocypack::Slice info)
    : RocksDBIndex(iid, coll, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::ZkdIndex),
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   coll.vocbase()
                       .server()
                       .getFeature<EngineSelectorFeature>()
                       .engine<RocksDBEngine>()),
      _storedValues(
          Index::parseFields(info.get(StaticStrings::IndexStoredValues),
                             /*allowEmpty*/ true, /*allowExpansion*/ false)),
      _sortedPrefixValues(Index::parseFields(info.get("sortedPrefixValues"),
                                             /*allowEmpty*/ true,
                                             /*allowExpansion*/ false)),
      _coveredFields(Index::mergeFields(_sortedPrefixValues, _storedValues)) {}

void RocksDBZkdIndexBase::toVelocyPack(
    velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type type) const {
  VPackObjectBuilder ob(&builder);
  RocksDBIndex::toVelocyPack(builder, type);

  if (!_storedValues.empty()) {
    builder.add(velocypack::Value(StaticStrings::IndexStoredValues));
    builder.openArray();

    for (auto const& field : _storedValues) {
      std::string fieldString;
      TRI_AttributeNamesToString(field, fieldString);
      builder.add(VPackValue(fieldString));
    }

    builder.close();
  }
  if (!_sortedPrefixValues.empty()) {
    builder.add(velocypack::Value("sortedPrefixValues"));
    builder.openArray();

    for (auto const& field : _sortedPrefixValues) {
      std::string fieldString;
      TRI_AttributeNamesToString(field, fieldString);
      builder.add(VPackValue(fieldString));
    }

    builder.close();
  }
}

Index::FilterCosts RocksDBZkdIndexBase::supportsFilterCondition(
    transaction::Methods& /*trx*/,
    const std::vector<std::shared_ptr<Index>>& allIndexes,
    const aql::AstNode* node, const aql::Variable* reference,
    size_t itemsInIndex) const {
  return zkd::supportsFilterCondition(this, allIndexes, node, reference,
                                      itemsInIndex);
}

aql::AstNode* RocksDBZkdIndexBase::specializeCondition(
    transaction::Methods& /*trx*/, aql::AstNode* condition,
    const aql::Variable* reference) const {
  return zkd::specializeCondition(this, condition, reference);
}

std::unique_ptr<IndexIterator> RocksDBZkdIndexBase::iteratorForCondition(
    ResourceMonitor& monitor, transaction::Methods* trx,
    const aql::AstNode* node, const aql::Variable* reference,
    const IndexIteratorOptions& opts, ReadOwnWrites readOwnWrites, int) {
  transaction::BuilderLeaser leaser(trx);
  auto&& [min, max] = boundsForIterator(this, node, reference, opts, *leaser);

  return std::make_unique<RocksDBZkdIndexIterator<false>>(
      monitor, &_collection, this, trx, std::move(min), std::move(max),
      std::move(leaser), fields().size(), readOwnWrites, opts.lookahead);
}

std::unique_ptr<IndexIterator> RocksDBUniqueZkdIndex::iteratorForCondition(
    ResourceMonitor& monitor, transaction::Methods* trx,
    const aql::AstNode* node, const aql::Variable* reference,
    const IndexIteratorOptions& opts, ReadOwnWrites readOwnWrites, int) {
  transaction::BuilderLeaser leaser(trx);
  auto&& [min, max] = boundsForIterator(this, node, reference, opts, *leaser);

  return std::make_unique<RocksDBZkdIndexIterator<true>>(
      monitor, &_collection, this, trx, std::move(min), std::move(max),
      std::move(leaser), fields().size(), readOwnWrites, opts.lookahead);
}

Result RocksDBUniqueZkdIndex::insert(transaction::Methods& trx,
                                     RocksDBMethods* methods,
                                     LocalDocumentId documentId,
                                     velocypack::Slice doc,
                                     OperationOptions const& options,
                                     bool performChecks) {
  TRI_ASSERT(_unique == true);
  TRI_ASSERT(_sparse == false);

  // TODO what about performChecks
  auto key_value = readDocumentKey(doc, _fields);
  auto prefixValues = extractAttributeValues(trx, _sortedPrefixValues, doc);

  RocksDBKey rocks_key;
  rocks_key.constructZkdIndexValue(objectId(), prefixValues->slice(),
                                   key_value);

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

  auto storedValues = extractAttributeValues(trx, _storedValues, doc);
  auto value =
      RocksDBValue::UniqueZkdIndexValue(documentId, storedValues->slice());

  if (auto s = methods->PutUntracked(_cf, rocks_key, value.string()); !s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}

Result RocksDBUniqueZkdIndex::remove(transaction::Methods& trx,
                                     RocksDBMethods* methods,
                                     LocalDocumentId documentId,
                                     velocypack::Slice doc,
                                     OperationOptions const& /*options*/) {
  TRI_ASSERT(_unique == true);
  TRI_ASSERT(_sparse == false);

  auto key_value = readDocumentKey(doc, _fields);
  auto prefixValues = extractAttributeValues(trx, _sortedPrefixValues, doc);

  RocksDBKey rocks_key;
  rocks_key.constructZkdIndexValue(objectId(), prefixValues->slice(),
                                   key_value);

  auto s = methods->SingleDelete(_cf, rocks_key);
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}
