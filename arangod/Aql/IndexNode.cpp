////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "IndexNode.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IndexBlock.h"
#include "Aql/Query.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

/// @brief constructor
IndexNode::IndexNode(ExecutionPlan* plan, size_t id,
                     Collection const* collection, Variable const* outVariable,
                     std::vector<transaction::Methods::IndexHandle> const& indexes,
                     std::unique_ptr<Condition> condition, IndexIteratorOptions const& opts)
    : ExecutionNode(plan, id),
      DocumentProducingNode(outVariable),
      CollectionAccessingNode(collection),
      _indexes(indexes),
      _condition(std::move(condition)),
      _needsGatherNodeSort(false),
      _options(opts) {
  TRI_ASSERT(_condition != nullptr);

  initIndexCoversProjections();
}

/// @brief constructor for IndexNode
IndexNode::IndexNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      DocumentProducingNode(plan, base),
      CollectionAccessingNode(plan, base),
      _indexes(),
      _needsGatherNodeSort(
          basics::VelocyPackHelper::readBooleanValue(base, "needsGatherNodeSort", false)),
      _options() {
  _options.sorted = basics::VelocyPackHelper::readBooleanValue(base, "sorted", true);
  _options.ascending =
      basics::VelocyPackHelper::readBooleanValue(base, "ascending", false);
  _options.evaluateFCalls =
      basics::VelocyPackHelper::readBooleanValue(base, "evalFCalls", true);
  _options.limit = basics::VelocyPackHelper::readNumericValue(base, "limit", 0);

  if (_options.sorted && base.isObject() && base.get("reverse").isBool()) {
    // legacy
    _options.sorted = true;
    _options.ascending = !(base.get("reverse").getBool());
  }

  VPackSlice indexes = base.get("indexes");

  if (!indexes.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "\"indexes\" attribute should be an array");
  }

  _indexes.reserve(indexes.length());

  auto trx = plan->getAst()->query()->trx();
  for (VPackSlice it : VPackArrayIterator(indexes)) {
    std::string iid = it.get("id").copyString();
    _indexes.emplace_back(trx->getIndexByIdentifier(_collection->name(), iid));
  }

  VPackSlice condition = base.get("condition");
  if (!condition.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER, "\"condition\" attribute should be an object");
  }

  _condition.reset(Condition::fromVPack(plan, condition));

  TRI_ASSERT(_condition != nullptr);

  initIndexCoversProjections();
}

/// @brief called to build up the matching positions of the index values for
/// the projection attributes (if any)
void IndexNode::initIndexCoversProjections() {
  _coveringIndexAttributePositions.clear();

  if (_indexes.empty()) {
    // no indexes used
    return;
  }

  // cannot apply the optimization if we use more than one different index
  auto idx = _indexes[0].getIndex();
  for (size_t i = 1; i < _indexes.size(); ++i) {
    if (_indexes[i].getIndex() != idx) {
      // different index used => optimization not possible
      return;
    }
  }

  // note that we made sure that if we have multiple index instances, they
  // are actually all of the same index

  auto const& fields = idx->coveredFields();

  if (!idx->hasCoveringIterator()) {
    // index does not have a covering index iterator
    return;
  }

  // check if we can use covering indexes
  if (fields.size() < projections().size()) {
    // we will not be able to satisfy all requested projections with this index
    return;
  }

  std::vector<size_t> coveringAttributePositions;
  // test if the index fields are the same fields as used in the projection
  std::string result;
  size_t i = 0;
  for (auto const& it : projections()) {
    bool found = false;
    for (size_t j = 0; j < fields.size(); ++j) {
      result.clear();
      TRI_AttributeNamesToString(fields[j], result, false);
      if (result == it) {
        found = true;
        coveringAttributePositions.emplace_back(j);
        break;
      }
    }
    if (!found) {
      return;
    }
    ++i;
  }

  _coveringIndexAttributePositions = std::move(coveringAttributePositions);
}

/// @brief toVelocyPack, for IndexNode
void IndexNode::toVelocyPackHelper(VPackBuilder& builder, unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(builder, flags);

  // add outvariable and projections
  DocumentProducingNode::toVelocyPack(builder);

  // add collection information
  CollectionAccessingNode::toVelocyPack(builder);

  // Now put info about vocbase and cid in there
  builder.add("needsGatherNodeSort", VPackValue(_needsGatherNodeSort));
  builder.add("indexCoversProjections",
              VPackValue(!_coveringIndexAttributePositions.empty()));

  builder.add(VPackValue("indexes"));
  {
    VPackArrayBuilder guard(&builder);
    for (auto& index : _indexes) {
      index.toVelocyPack(builder, Index::makeFlags(Index::Serialize::Estimates));
    }
  }
  builder.add(VPackValue("condition"));
  _condition->toVelocyPack(builder, flags);
  // IndexIteratorOptions
  builder.add("sorted", VPackValue(_options.sorted));
  builder.add("ascending", VPackValue(_options.ascending));
  builder.add("reverse", VPackValue(!_options.ascending));  // legacy
  builder.add("evalFCalls", VPackValue(_options.evaluateFCalls));
  builder.add("limit", VPackValue(_options.limit));

  // And close it:
  builder.close();
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> IndexNode::createBlock(
    ExecutionEngine& engine, std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const {
  return std::make_unique<IndexBlock>(&engine, this);
}

ExecutionNode* IndexNode::clone(ExecutionPlan* plan, bool withDependencies,
                                bool withProperties) const {
  auto outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }

  auto c = std::make_unique<IndexNode>(plan, _id, _collection, outVariable, _indexes,
                                       std::unique_ptr<Condition>(_condition->clone()),
                                       _options);

  c->projections(_projections);
  c->needsGatherNodeSort(_needsGatherNodeSort);
  c->initIndexCoversProjections();
  c->_prototypeCollection = _prototypeCollection;
  c->_prototypeOutVariable = _prototypeOutVariable;

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

/// @brief destroy the IndexNode
IndexNode::~IndexNode() {}

/// @brief the cost of an index node is a multiple of the cost of
/// its unique dependency
CostEstimate IndexNode::estimateCost() const {
  CostEstimate estimate = _dependencies.at(0)->getCost();
  size_t incoming = estimate.estimatedNrItems;

  transaction::Methods* trx = _plan->getAst()->query()->trx();
  // estimate for the number of documents in the collection. may be outdated...
  size_t const itemsInCollection = _collection->count(trx);
  size_t totalItems = 0;
  double totalCost = 0.0;

  auto root = _condition->root();

  for (size_t i = 0; i < _indexes.size(); ++i) {
    double estimatedCost = 0.0;
    size_t estimatedItems = 0;

    arangodb::aql::AstNode const* condition;
    if (root == nullptr || root->numMembers() <= i) {
      condition = nullptr;
    } else {
      condition = root->getMember(i);
    }

    if (condition != nullptr &&
        trx->supportsFilterCondition(_indexes[i], condition, _outVariable, itemsInCollection,
                                     estimatedItems, estimatedCost)) {
      totalItems += estimatedItems;
      totalCost += estimatedCost;
    } else {
      totalItems += itemsInCollection;
      totalCost += static_cast<double>(itemsInCollection);
    }
  }

  estimate.estimatedNrItems *= totalItems;
  estimate.estimatedCost += incoming * totalCost;
  return estimate;
}

/// @brief getVariablesUsedHere, returning a vector
std::vector<Variable const*> IndexNode::getVariablesUsedHere() const {
  std::unordered_set<Variable const*> s;
  // actual work is done by that method
  getVariablesUsedHere(s);

  // copy result into vector
  std::vector<Variable const*> v;
  v.reserve(s.size());

  for (auto const& vv : s) {
    v.emplace_back(const_cast<Variable*>(vv));
  }
  return v;
}

/// @brief getVariablesUsedHere, modifying the set in-place
void IndexNode::getVariablesUsedHere(std::unordered_set<Variable const*>& vars) const {
  Ast::getReferencedVariables(_condition->root(), vars);

  vars.erase(_outVariable);
}
