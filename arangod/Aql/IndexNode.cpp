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
#include "Aql/Query.h"
#include "Transaction/Methods.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

/// @brief constructor
IndexNode::IndexNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
            Collection const* collection, Variable const* outVariable,
            std::vector<transaction::Methods::IndexHandle> const& indexes,
            Condition* condition, bool reverse)
      : ExecutionNode(plan, id),
        DocumentProducingNode(outVariable),
        _vocbase(vocbase),
        _collection(collection),
        _indexes(indexes),
        _condition(condition),
        _reverse(reverse) {
  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_collection != nullptr);
  TRI_ASSERT(_condition != nullptr);
}

/// @brief constructor for IndexNode 
IndexNode::IndexNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base) 
    : ExecutionNode(plan, base),
      DocumentProducingNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          base.get("collection").copyString())),
      _indexes(),
      _condition(nullptr),
      _reverse(base.get("reverse").getBoolean()) {
  VPackSlice indexes = base.get("indexes");

  if (!indexes.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "\"indexes\" attribute should be an array");
  }

  _indexes.reserve(indexes.length());

  auto trx = plan->getAst()->query()->trx();
  for (auto const& it : VPackArrayIterator(indexes)) {
    std::string iid  = it.get("id").copyString();
    _indexes.emplace_back(trx->getIndexByIdentifier(_collection->getName(), iid));
  }

  VPackSlice condition = base.get("condition");
  if (!condition.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "\"condition\" attribute should be an object");
  }

  _condition = Condition::fromVPack(plan, condition);

  TRI_ASSERT(_condition != nullptr);
}

/// @brief toVelocyPack, for IndexNode
void IndexNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

  // Now put info about vocbase and cid in there
  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("collection", VPackValue(_collection->getName()));
  nodes.add("satellite", VPackValue(_collection->isSatellite()));
  
  // add outvariable and projection
  DocumentProducingNode::toVelocyPack(nodes);
  
  nodes.add(VPackValue("indexes"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto& index : _indexes) {
      index.toVelocyPack(nodes, false);
    }
  }
  nodes.add(VPackValue("condition"));
  _condition->toVelocyPack(nodes, verbose);
  nodes.add("reverse", VPackValue(_reverse));

  // And close it:
  nodes.close();
}

ExecutionNode* IndexNode::clone(ExecutionPlan* plan, bool withDependencies,
                                bool withProperties) const {
  auto outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }

  auto c = new IndexNode(plan, _id, _vocbase, _collection, outVariable,
                         _indexes, _condition->clone(), _reverse);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

/// @brief destroy the IndexNode
IndexNode::~IndexNode() { delete _condition; }

/// @brief the cost of an index node is a multiple of the cost of
/// its unique dependency
double IndexNode::estimateCost(size_t& nrItems) const {
  size_t incoming = 0;
  double const dependencyCost = _dependencies.at(0)->getCost(incoming);
  transaction::Methods* trx = _plan->getAst()->query()->trx();
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
        trx->supportsFilterCondition(_indexes[i], condition,
                                     _outVariable, itemsInCollection,
                                     estimatedItems, estimatedCost)) {
      totalItems += estimatedItems;
      totalCost += estimatedCost;
    } else {
      totalItems += itemsInCollection;
      totalCost += static_cast<double>(itemsInCollection);
    }
  }

  nrItems = incoming * totalItems;
  return dependencyCost + incoming * totalCost;
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
void IndexNode::getVariablesUsedHere(
    std::unordered_set<Variable const*>& vars) const {
  Ast::getReferencedVariables(_condition->root(), vars);

  vars.erase(_outVariable);
}
