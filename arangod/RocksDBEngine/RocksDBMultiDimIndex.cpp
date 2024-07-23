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
/// @author Tobias Gödderz
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBMultiDimIndex.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "ClusterEngine/ClusterIndex.h"
#include "Containers/Enumerate.h"
#include "Containers/FlatHashSet.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "Zkd/ZkdHelper.h"

using namespace arangodb;

namespace arangodb {

template<bool isUnique = false, bool hasPrefix = false>
class RocksDBMdiIndexIterator final : public IndexIterator {
 public:
  RocksDBMdiIndexIterator(ResourceMonitor& monitor,
                          LogicalCollection* collection,
                          RocksDBMdiIndexBase* index, transaction::Methods* trx,
                          zkd::byte_string min, zkd::byte_string max,
                          transaction::BuilderLeaser prefix, std::size_t dim,
                          ReadOwnWrites readOwnWrites, size_t lookahead)
      : IndexIterator(collection, trx, readOwnWrites),
        _min(std::move(min)),
        _max(std::move(max)),
        _bound(RocksDBKeyBounds::MdiIndex(index->objectId())),
        _dim(dim),
        _prefix(std::move(prefix)),
        _index(index),
        _lookahead(lookahead) {
    _cur = _min;

    TRI_ASSERT(hasPrefix == !_prefix->isEmpty());

    if constexpr (hasPrefix) {
      VPackBuilder builder;
      {
        VPackArrayBuilder ab(&builder);
        builder.add(VPackArrayIterator(_prefix->slice()));
        builder.add(VPackSlice::maxKeySlice());
      }

      _upperBoundKey.constructMdiIndexValue(index->objectId(), builder.slice(),
                                            {});
      _upperBound = _upperBoundKey.string();
    } else {
      _upperBound = _bound.end();
    }

    RocksDBTransactionMethods* mthds =
        RocksDBTransactionState::toMethods(trx, _collection->id());
    _iter = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
      TRI_ASSERT(opts.prefix_same_as_start);
      opts.iterate_upper_bound = &_upperBound;
    });
    TRI_ASSERT(_iter != nullptr);
    _compareResult.resize(_dim);
  }

  std::string_view typeName() const noexcept final {
    return "rocksdb-mdi-index-iterator";
  }

 protected:
  size_t numNextTries()
      const noexcept {  // may depend on the number of dimensions
                        // and the limits of the query
    return _lookahead;
  }

  static auto getCurveValue(rocksdb::Slice key) {
    if constexpr (hasPrefix) {
      if constexpr (isUnique) {
        return RocksDBKey::mdiUniqueVPackIndexCurveValue(key);
      } else {
        return RocksDBKey::mdiVPackIndexCurveValue(key);
      }
    } else {
      if constexpr (isUnique) {
        return RocksDBKey::mdiUniqueIndexCurveValue(key);
      } else {
        return RocksDBKey::mdiIndexCurveValue(key);
      }
    }
  }

  auto loadKey(zkd::byte_string_view key) {
    if constexpr (hasPrefix) {
      _rocksdbKey.constructMdiIndexValue(_index->objectId(), _prefix->slice(),
                                         _cur);
    } else {
      _rocksdbKey.constructMdiIndexValue(_index->objectId(), _cur);
    }
  }

  template<typename F>
  bool findNext(F&& callback, uint64_t limit) {
    for (uint64_t i = 0; i < limit;) {
      switch (_iterState) {
        case IterState::SEEK_ITER_TO_CUR: {
          loadKey(_cur);
          _iter->Seek(_rocksdbKey.string());

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
          auto byteStringKey = getCurveValue(rocksKey);

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
            byteStringKey = getCurveValue(rocksKey);
            foundNextZValueInBox =
                zkd::testInBox(byteStringKey, _min, _max, _dim);
          }

          if (_iterState == IterState::DONE) {
            break;  // case CHECK_CURRENT_ITER
          }

          if (!foundNextZValueInBox) {
            zkd::compareWithBoxInto(byteStringKey, _min, _max, _dim,
                                    _compareResult);
            auto const next =
                zkd::getNextZValue(byteStringKey, _min, _max, _compareResult);
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
      CoveringData(VPackSlice prefixValues, VPackSlice storedValues)
          : _storedValues(storedValues),
            _prefixValuesLength(prefixValues.length()),
            _prefixValues(prefixValues) {}
      VPackSlice at(size_t i) const override {
        if (i < _prefixValuesLength) {
          return _prefixValues.at(i);
        }
        return _storedValues.at(i - _prefixValuesLength);
      }
      bool isArray() const noexcept override { return true; }
      velocypack::ValueLength length() const override {
        return _prefixValuesLength + _storedValues.length();
      }

      velocypack::Slice _storedValues;
      std::size_t _prefixValuesLength;
      velocypack::Slice _prefixValues;
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
          auto prefixValues = std::invoke([&] {
            if constexpr (hasPrefix) {
              return RocksDBKey::indexedVPack(key);
            } else {
              return VPackSlice::emptyArraySlice();
            }
          });
          CoveringData coveringData{prefixValues, storedValues};
          std::ignore = callback(documentId, coveringData);
        },
        limit);
  }

 private:
  RocksDBKey _rocksdbKey;
  rocksdb::Slice _upperBound;
  RocksDBKey _upperBoundKey;
  zkd::byte_string _cur;
  zkd::byte_string const _min;
  zkd::byte_string const _max;
  RocksDBKeyBounds _bound;
  std::size_t const _dim;
  transaction::BuilderLeaser const _prefix;

  enum class IterState {
    SEEK_ITER_TO_CUR = 0,
    CHECK_CURRENT_ITER,
    DONE,
  };
  IterState _iterState = IterState::SEEK_ITER_TO_CUR;

  std::unique_ptr<rocksdb::Iterator> _iter;
  RocksDBMdiIndexBase* _index = nullptr;

  size_t const _lookahead;

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

ResultT<zkd::byte_string> readDocumentKey(
    VPackSlice doc,
    std::vector<std::vector<basics::AttributeName>> const& fields) {
  std::vector<zkd::byte_string> v;
  v.reserve(fields.size());

  for (auto const& path : fields) {
    VPackSlice value = accessDocumentPath(doc, path);
    if (!value.isNumber<double>()) {
      return {TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE};
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

auto boundsForIterator(RocksDBMdiIndexBase const* index,
                       aql::AstNode const* node, aql::Variable const* reference,
                       IndexIteratorOptions const& opts,
                       velocypack::Builder& prefixValuesBuilder)
    -> std::pair<zkd::byte_string, zkd::byte_string> {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
  std::unordered_map<size_t, aql::AstNode const*> extractedPrefix;
  std::unordered_map<size_t, mdi::ExpressionBounds> extractedBounds;
  std::unordered_set<aql::AstNode const*> unusedExpressions;
  extractBoundsFromCondition(index, node, reference, extractedPrefix,
                             extractedBounds, unusedExpressions);

  TRI_ASSERT(unusedExpressions.empty());

  size_t const dim = index->fields().size();
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
  if (!index->prefixFields().empty()) {
    prefixValuesBuilder.openArray();
    for (auto&& [idx, field] : enumerate(index->prefixFields())) {
      auto it = extractedPrefix.find(idx);
      TRI_ASSERT(it != extractedPrefix.end())
          << "Field `" << field << "` not found. Expr: " << node->toString()
          << " Fields: " << index->prefixFields();
      aql::AstNode const* value = it->second;
      TRI_ASSERT(value->isConstant())
          << "Value is not constant: " << value->toString();
      value->toVelocyPackValue(prefixValuesBuilder);
    }
    prefixValuesBuilder.close();
  }

  TRI_ASSERT(min.size() == dim);
  TRI_ASSERT(max.size() == dim);

  return std::make_pair(zkd::interleave(min), zkd::interleave(max));
}

std::vector<std::vector<basics::AttributeName>> const& getSortedPrefixFields(
    Index const* index) {
  if (auto ptr = dynamic_cast<RocksDBMdiIndexBase const*>(index);
      ptr != nullptr) {
    return ptr->prefixFields();
  }
  if (auto ptr = dynamic_cast<ClusterIndex const*>(index); ptr != nullptr) {
    return ptr->prefixFields();
  }
  return Index::emptyCoveredFields;
}

}  // namespace

void mdi::extractBoundsFromCondition(
    Index const* index, aql::AstNode const* condition,
    aql::Variable const* reference,
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
      if (!inserted) {
        // duplicate equal condition, better not supported
        return false;
      }
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

auto mdi::supportsFilterCondition(
    Index const* index, std::vector<std::shared_ptr<Index>> const& allIndexes,
    aql::AstNode const* node, aql::Variable const* reference,
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

  Index::FilterCosts costs;
  costs.supportsCondition = true;
  costs.coveredAttributes = extractedBounds.size() + extractedPrefix.size();

  // we look up a single point using the prefix values
  auto const estimatedElementsOnCurve = [&]() -> double {
    if (index->hasSelectivityEstimate()) {
      auto estimate = index->selectivityEstimate();
      if (estimate > 0) {
        return 1. / estimate;
      }
    }

    return static_cast<double>(itemsInIndex);
  }();

  // each additional bound reduces the volume
  const double volumeReductionFactor = 1.4;  // guessed, 2 might be too much
  const double searchBoxVolume =
      1. /
      pow(volumeReductionFactor, static_cast<double>(extractedBounds.size()));

  costs.estimatedItems =
      static_cast<size_t>(estimatedElementsOnCurve * searchBoxVolume);

  const size_t unusedDimensions =
      index->fields().size() - extractedBounds.size();

  double const unusedDimensionCost =
      0.5 * static_cast<double>(unusedDimensions * costs.estimatedItems);
  auto const unusedExpressionCost =
      static_cast<double>(costs.estimatedItems * unusedExpressions.size());

  // account for post filtering
  costs.estimatedCosts = static_cast<double>(costs.estimatedItems) +
                         unusedDimensionCost + unusedExpressionCost;

  return costs;
}

auto mdi::specializeCondition(Index const* index, aql::AstNode* condition,
                              aql::Variable const* reference) -> aql::AstNode* {
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
ResultT<transaction::BuilderLeaser> extractAttributeValues(
    transaction::Methods& trx,
    std::vector<std::vector<basics::AttributeName>> const& storedValues,
    velocypack::Slice doc, bool nullAllowed) {
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
        if (!s.isObject()) {
          s = VPackSlice ::noneSlice();
          break;
        }
        s = s.get(part.name);
        if (s.isNone()) {
          break;
        }
      }
    }
    if (s.isNone()) {
      s = VPackSlice::nullSlice();
    }

    if (s.isNull() && !nullAllowed) {
      return {TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING};
    }

    leased->add(s);
  }
  leased->close();

  return leased;
}
}  // namespace

Result RocksDBMdiIndex::insert(transaction::Methods& trx,
                               RocksDBMethods* methods,
                               LocalDocumentId documentId,
                               velocypack::Slice doc,
                               OperationOptions const& options,
                               bool performChecks) {
  TRI_ASSERT(_unique == false);

  zkd::byte_string keyValue;
  {
    auto result = readDocumentKey(doc, _fields);
    if (result.fail()) {
      if (result.is(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE) && _sparse) {
        return {};
      }
      THROW_ARANGO_EXCEPTION(result.result());
    }
    keyValue = std::move(result.get());
  }

  RocksDBKey rocksdbKey;
  uint64_t hash = 0;
  if (!isPrefixed()) {
    rocksdbKey.constructMdiIndexValue(objectId(), keyValue, documentId);
  } else {
    auto result = extractAttributeValues(trx, _prefixFields, doc, !_sparse);
    if (result.fail()) {
      TRI_ASSERT(_sparse);
      TRI_ASSERT(result.is(TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING));
      return TRI_ERROR_NO_ERROR;
    }
    auto& prefixValues = result.get();
    rocksdbKey.constructMdiIndexValue(objectId(), prefixValues->slice(),
                                      keyValue, documentId);
    hash = _estimates ? prefixValues->slice().normalizedHash() : 0;
  }

  auto storedValues =
      std::move(extractAttributeValues(trx, _storedValues, doc, true).get());
  auto value = RocksDBValue::MdiIndexValue(storedValues->slice());
  auto s = methods->PutUntracked(_cf, rocksdbKey, value.string());
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  if (_estimates) {
    auto* state = RocksDBTransactionState::toState(&trx);
    auto* trxc = static_cast<RocksDBTransactionCollection*>(
        state->findCollection(_collection.id()));
    TRI_ASSERT(trxc != nullptr);
    trxc->trackIndexInsert(id(), hash);
  }

  return {};
}

void RocksDBMdiIndex::truncateCommit(TruncateGuard&& guard, TRI_voc_tick_t tick,
                                     transaction::Methods* trx) {
  if (_estimator != nullptr) {
    _estimator->bufferTruncate(tick);
  }
  RocksDBIndex::truncateCommit(std::move(guard), tick, trx);
}

Result RocksDBMdiIndex::drop() {
  Result res = RocksDBIndex::drop();

  if (res.ok() && _estimator != nullptr) {
    _estimator->drain();
  }

  return res;
}

Result RocksDBMdiIndex::remove(transaction::Methods& trx,
                               RocksDBMethods* methods,
                               LocalDocumentId documentId,
                               velocypack::Slice doc,
                               OperationOptions const& /*options*/) {
  TRI_ASSERT(_unique == false);

  zkd::byte_string keyValue;
  {
    auto result = readDocumentKey(doc, _fields);
    if (result.fail()) {
      if (result.is(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE) && _sparse) {
        return {};
      }
      THROW_ARANGO_EXCEPTION(result.result());
    }
    keyValue = std::move(result.get());
  }

  RocksDBKey rocksdbKey;
  uint64_t hash = 0;
  if (!isPrefixed()) {
    rocksdbKey.constructMdiIndexValue(objectId(), keyValue, documentId);
  } else {
    auto result = extractAttributeValues(trx, _prefixFields, doc, !_sparse);
    if (result.fail()) {
      TRI_ASSERT(_sparse);
      TRI_ASSERT(result.is(TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING));
      return TRI_ERROR_NO_ERROR;
    }
    auto& prefixValues = result.get();
    rocksdbKey.constructMdiIndexValue(objectId(), prefixValues->slice(),
                                      keyValue, documentId);
    hash = _estimates ? prefixValues->slice().normalizedHash() : 0;
  }

  auto s = methods->SingleDelete(_cf, rocksdbKey);
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  if (_estimates) {
    auto* state = RocksDBTransactionState::toState(&trx);
    auto* trxc = static_cast<RocksDBTransactionCollection*>(
        state->findCollection(_collection.id()));
    TRI_ASSERT(trxc != nullptr);
    // The estimator is only useful if we are in a non-unique index
    trxc->trackIndexRemove(id(), hash);
  }

  return {};
}

namespace {
auto columnFamilyForInfo(velocypack::Slice info) {
  if (auto prefix = info.get(StaticStrings::IndexPrefixFields);
      prefix.isArray() && !prefix.isEmptyArray()) {
    return RocksDBColumnFamilyManager::get(
        RocksDBColumnFamilyManager::Family::MdiVPackIndex);
  }

  return RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::MdiIndex);
}

uint64_t hashForKey(rocksdb::Slice key) {
  // NOTE: This function needs to use the same hashing on the
  // indexed VPack as the initial inserter does
  VPackSlice tmp = RocksDBKey::indexedVPack(key);
  return tmp.normalizedHash();
}

}  // namespace

RocksDBMdiIndexBase::RocksDBMdiIndexBase(IndexId iid, LogicalCollection& coll,
                                         velocypack::Slice info)
    : RocksDBIndex(iid, coll, info, columnFamilyForInfo(info),
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   coll.vocbase().engine<RocksDBEngine>()),
      _storedValues(
          Index::parseFields(info.get(StaticStrings::IndexStoredValues),
                             /*allowEmpty*/ true, /*allowExpansion*/ false)),
      _prefixFields(
          Index::parseFields(info.get(StaticStrings::IndexPrefixFields),
                             /*allowEmpty*/ true,
                             /*allowExpansion*/ false)),
      _coveredFields(Index::mergeFields(_prefixFields, _storedValues)),
      _type(Index::type(info.get(StaticStrings::IndexType).stringView())) {
  TRI_ASSERT(_type == TRI_IDX_TYPE_ZKD_INDEX ||
             _type == TRI_IDX_TYPE_MDI_INDEX ||
             _type == TRI_IDX_TYPE_MDI_PREFIXED_INDEX);
}

void RocksDBMdiIndexBase::toVelocyPack(
    velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type type) const {
  VPackObjectBuilder ob(&builder);
  RocksDBIndex::toVelocyPack(builder, type);
  builder.add("fieldValueTypes", VPackValue("double"));
  builder.add(StaticStrings::IndexEstimates,
              VPackValue(hasSelectivityEstimate()));
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
  if (!_prefixFields.empty()) {
    builder.add(velocypack::Value(StaticStrings::IndexPrefixFields));
    builder.openArray();

    for (auto const& field : _prefixFields) {
      std::string fieldString;
      TRI_AttributeNamesToString(field, fieldString);
      builder.add(VPackValue(fieldString));
    }

    builder.close();
  }
}

/// @brief Test if this index matches the definition
bool RocksDBMdiIndexBase::matchesDefinition(VPackSlice const& info) const {
  // call compare method of parent first
  if (!RocksDBIndex::matchesDefinition(info)) {
    return false;
  }
  // compare prefix values
  auto value = info.get(arangodb::StaticStrings::IndexPrefixFields);

  if (value.isNone()) {
    return _prefixFields.empty();
  }

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _prefixFields.size()) {
    return false;
  }

  std::vector<arangodb::basics::AttributeName> translate;
  for (size_t i = 0; i < n; ++i) {
    translate.clear();
    VPackSlice f = value.at(i);
    if (!f.isString()) {
      // Invalid field definition!
      return false;
    }
    TRI_ParseAttributeString(f.stringView(), translate, true);
    if (!arangodb::basics::AttributeName::isIdentical(_prefixFields[i],
                                                      translate, false)) {
      return false;
    }
  }
  return true;
}

bool RocksDBMdiIndex::hasSelectivityEstimate() const {
  TRI_ASSERT(!_unique);
  return _estimates && isPrefixed();
}

double RocksDBMdiIndex::selectivityEstimate(std::string_view) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(!_unique);
  if (_estimator == nullptr || !_estimates) {
    // we turn off the estimates for some system collections to avoid updating
    // them too often. we also turn off estimates for stub collections on
    // coordinator and DB servers
    return 0.0;
  }
  TRI_ASSERT(_estimator != nullptr);
  return _estimator->computeEstimate();
}

RocksDBCuckooIndexEstimatorType* RocksDBMdiIndex::estimator() {
  return _estimator.get();
}

void RocksDBMdiIndex::setEstimator(
    std::unique_ptr<RocksDBCuckooIndexEstimatorType> est) {
  TRI_ASSERT(!_unique);
  TRI_ASSERT(_estimator == nullptr ||
             _estimator->appliedSeq() <= est->appliedSeq());
  _estimator = std::move(est);
}

void RocksDBMdiIndex::recalculateEstimates() {
  if (unique() || _estimator == nullptr) {
    return;
  }
  TRI_ASSERT(_estimator != nullptr);
  _estimator->clear();

  auto& engine = _collection.vocbase().engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  rocksdb::SequenceNumber seq = db->GetLatestSequenceNumber();

  auto bounds = getBounds();
  rocksdb::Slice const end = bounds.end();
  rocksdb::ReadOptions options;
  options.iterate_upper_bound = &end;  // safe to use on rocksb::DB directly
  options.prefix_same_as_start = true;
  options.verify_checksums = false;
  options.fill_cache = false;
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(options, _cf));
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    uint64_t hash = hashForKey(it->key());
    // cppcheck-suppress uninitvar ; doesn't understand above call
    _estimator->insert(hash);
  }
  _estimator->setAppliedSeq(seq);
}

Index::FilterCosts RocksDBMdiIndexBase::supportsFilterCondition(
    transaction::Methods& /*trx*/,
    std::vector<std::shared_ptr<Index>> const& allIndexes,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) const {
  return mdi::supportsFilterCondition(this, allIndexes, node, reference,
                                      itemsInIndex);
}

aql::AstNode* RocksDBMdiIndexBase::specializeCondition(
    transaction::Methods& /*trx*/, aql::AstNode* condition,
    aql::Variable const* reference) const {
  return mdi::specializeCondition(this, condition, reference);
}

Index::IndexType RocksDBMdiIndexBase::type() const { return _type; }

char const* RocksDBMdiIndexBase::typeName() const {
  return Index::oldtypeName(type());
}

std::unique_ptr<IndexIterator> RocksDBMdiIndex::iteratorForCondition(
    ResourceMonitor& monitor, transaction::Methods* trx,
    aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites, int) {
  transaction::BuilderLeaser leaser(trx);
  auto&& [min, max] = boundsForIterator(this, node, reference, opts, *leaser);

  if (!isPrefixed()) {
    return std::make_unique<RocksDBMdiIndexIterator<false, false>>(
        monitor, &_collection, this, trx, std::move(min), std::move(max),
        std::move(leaser), fields().size(), readOwnWrites, opts.lookahead);
  } else {
    return std::make_unique<RocksDBMdiIndexIterator<false, true>>(
        monitor, &_collection, this, trx, std::move(min), std::move(max),
        std::move(leaser), fields().size(), readOwnWrites, opts.lookahead);
  }
}

RocksDBMdiIndex::RocksDBMdiIndex(IndexId iid, LogicalCollection& coll,
                                 velocypack::Slice info)
    : RocksDBMdiIndexBase(iid, coll, info), _estimates(true) {
  TRI_ASSERT(!_unique);
  if (VPackSlice s = info.get(StaticStrings::IndexEstimates); s.isBoolean()) {
    // read "estimates" flag from velocypack if it is present.
    // if it's not present, we go with the default (estimates = true)
    _estimates = s.getBoolean();
  }

  if (!isPrefixed()) {
    _estimates = false;
  }

  if (_estimates && !ServerState::instance()->isCoordinator() &&
      !coll.isAStub()) {
    // We activate the estimator for all non unique-indexes.
    // And only on single servers and DBServers
    _estimator = std::make_unique<RocksDBCuckooIndexEstimatorType>(
        &coll.vocbase()
             .engine<RocksDBEngine>()
             .indexEstimatorMemoryUsageMetric(),
        RocksDBIndex::ESTIMATOR_SIZE);
  }
}

std::unique_ptr<IndexIterator> RocksDBUniqueMdiIndex::iteratorForCondition(
    ResourceMonitor& monitor, transaction::Methods* trx,
    aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites, int) {
  transaction::BuilderLeaser leaser(trx);
  auto&& [min, max] = boundsForIterator(this, node, reference, opts, *leaser);

  if (!isPrefixed()) {
    return std::make_unique<RocksDBMdiIndexIterator<true, false>>(
        monitor, &_collection, this, trx, std::move(min), std::move(max),
        std::move(leaser), fields().size(), readOwnWrites, opts.lookahead);
  } else {
    return std::make_unique<RocksDBMdiIndexIterator<true, true>>(
        monitor, &_collection, this, trx, std::move(min), std::move(max),
        std::move(leaser), fields().size(), readOwnWrites, opts.lookahead);
  }
}

Result RocksDBUniqueMdiIndex::insert(transaction::Methods& trx,
                                     RocksDBMethods* methods,
                                     LocalDocumentId documentId,
                                     velocypack::Slice doc,
                                     OperationOptions const& options,
                                     bool performChecks) {
  TRI_ASSERT(_unique == true);

  zkd::byte_string keyValue;
  {
    auto result = readDocumentKey(doc, _fields);
    if (result.fail()) {
      if (result.is(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE) && _sparse) {
        return {};
      }
      THROW_ARANGO_EXCEPTION(result.result());
    }
    keyValue = std::move(result.get());
  }

  RocksDBKey rocksdbKey;
  if (!isPrefixed()) {
    rocksdbKey.constructMdiIndexValue(objectId(), keyValue);
  } else {
    auto result = extractAttributeValues(trx, _prefixFields, doc, !_sparse);
    if (result.fail()) {
      TRI_ASSERT(_sparse);
      TRI_ASSERT(result.is(TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING));
      return TRI_ERROR_NO_ERROR;
    }
    auto& prefixValues = result.get();
    rocksdbKey.constructMdiIndexValue(objectId(), prefixValues->slice(),
                                      keyValue);
  }

  if (!options.checkUniqueConstraintsInPreflight) {
    transaction::StringLeaser leased(&trx);
    rocksdb::PinnableSlice existing(leased.get());
    if (auto s = methods->GetForUpdate(_cf, rocksdbKey.string(), &existing);
        s.ok()) {  // detected conflicting index entry
      return {TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED};
    } else if (!s.IsNotFound()) {
      return Result(rocksutils::convertStatus(s));
    }
  }

  auto storedValues =
      std::move(extractAttributeValues(trx, _storedValues, doc, true).get());
  auto value =
      RocksDBValue::UniqueMdiIndexValue(documentId, storedValues->slice());

  if (auto s = methods->PutUntracked(_cf, rocksdbKey, value.string());
      !s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}

Result RocksDBUniqueMdiIndex::remove(transaction::Methods& trx,
                                     RocksDBMethods* methods,
                                     LocalDocumentId documentId,
                                     velocypack::Slice doc,
                                     OperationOptions const& /*options*/) {
  TRI_ASSERT(_unique == true);

  zkd::byte_string keyValue;
  {
    auto result = readDocumentKey(doc, _fields);
    if (result.fail()) {
      if (result.is(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE) && _sparse) {
        return {};
      }
      THROW_ARANGO_EXCEPTION(result.result());
    }
    keyValue = std::move(result.get());
  }

  RocksDBKey rocksdbKey;
  if (!isPrefixed()) {
    rocksdbKey.constructMdiIndexValue(objectId(), keyValue);
  } else {
    auto result = extractAttributeValues(trx, _prefixFields, doc, !_sparse);
    if (result.fail()) {
      TRI_ASSERT(_sparse);
      TRI_ASSERT(result.is(TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING));
      return TRI_ERROR_NO_ERROR;
    }
    auto& prefixValues = result.get();
    rocksdbKey.constructMdiIndexValue(objectId(), prefixValues->slice(),
                                      keyValue);
  }

  auto s = methods->SingleDelete(_cf, rocksdbKey);
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  return {};
}
