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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ExecutionNode/DistributeNode.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/DocumentProducingNode.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/DistributeExecutor.h"
#include "Aql/Executor/IdExecutor.h"
#include "Aql/Query.h"
#include "Aql/types.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Iterator.h>

#include <memory>
#include <string_view>
#include <type_traits>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::aql;

DistributeNode::DistributeNode(ExecutionPlan* plan, ExecutionNodeId id,
                               ScatterNode::ScatterType type,
                               Collection const* collection,
                               Variable const* variable,
                               ExecutionNodeId targetNodeId)
    : ScatterNode(plan, id, type),
      CollectionAccessingNode(collection),
      _variable(variable),
      _attribute(determineProjectionAttribute()),
      _targetNodeId(targetNodeId) {}

/// @brief construct a distribute node
DistributeNode::DistributeNode(ExecutionPlan* plan,
                               arangodb::velocypack::Slice const& base)
    : ScatterNode(plan, base),
      CollectionAccessingNode(plan, base),
      _variable(Variable::varFromVPack(plan->getAst(), base, "variable")) {
  if (auto sats = base.get("satelliteCollections"); sats.isArray()) {
    auto& queryCols = plan->getAst()->query().collections();
    _satellites.reserve(sats.length());

    for (VPackSlice it : VPackArrayIterator(sats)) {
      std::string v =
          arangodb::basics::VelocyPackHelper::getStringValue(it, "");
      auto c = queryCols.add(v, AccessMode::Type::READ,
                             aql::Collection::Hint::Collection);
      addSatellite(c);
    }
  }

  if (auto attr = base.get("attribute"); attr.isArray()) {
    for (VPackSlice it : VPackArrayIterator(attr)) {
      _attribute.push_back(it.copyString());
    }
  }
}

/// @brief clone ExecutionNode recursively
ExecutionNode* DistributeNode::clone(ExecutionPlan* plan,
                                     bool withDependencies) const {
  auto c = std::make_unique<DistributeNode>(
      plan, _id, getScatterType(), collection(), _variable, _targetNodeId);
  c->copyClients(clients());
  CollectionAccessingNode::cloneInto(*c);
  c->_satellites.reserve(_satellites.size());
  for (auto& it : _satellites) {
    c->_satellites.emplace_back(it);
  }

  return cloneHelper(std::move(c), withDependencies);
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> DistributeNode::createBlock(
    ExecutionEngine& engine) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  // get the variable to inspect . . .
  TRI_ASSERT(_variable != nullptr);
  VariableId varId = _variable->id;

  // get the register id of the variable to inspect . . .
  auto it = getRegisterPlan()->varInfo.find(varId);
  TRI_ASSERT(it != getRegisterPlan()->varInfo.end());
  RegisterId regId = (*it).second.registerId;

  TRI_ASSERT(regId.isValid());

  auto inRegs = RegIdSet{regId};
  auto registerInfos = createRegisterInfos(std::move(inRegs), {});
  auto infos =
      DistributeExecutorInfos(clients(), collection(), regId, _attribute,
                              getScatterType(), getSatellites());

  return std::make_unique<ExecutionBlockImpl<DistributeExecutor>>(
      &engine, this, std::move(registerInfos), std::move(infos));
}

void DistributeNode::setVariable(Variable const* var) {
  _variable = var;
  determineProjectionAttribute();
}

std::vector<std::string> DistributeNode::determineProjectionAttribute() const {
  std::vector<std::string> attribute;
  // check where input variable of DISTRIBUTE comes from.
  // if it is from a projection, we also look up the projection's
  // attribute name
  TRI_ASSERT(_variable != nullptr);
  auto setter = plan()->getVarSetBy(_variable->id);
  if (setter != nullptr &&
      (setter->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
       setter->getType() == ExecutionNode::INDEX)) {
    DocumentProducingNode const* document =
        dynamic_cast<DocumentProducingNode const*>(setter);
    auto const& p = document->projections();
    for (size_t i = 0; i < p.size(); ++i) {
      if (p[i].variable == _variable) {
        // found the DISTRIBUTE input variable in a projection
        attribute = p[i].path.get();
        break;
      }
    }
  }

  return attribute;
}

/// @brief doToVelocyPack, for DistributeNode
void DistributeNode::doToVelocyPack(VPackBuilder& builder,
                                    unsigned flags) const {
  // call base class method
  ScatterNode::doToVelocyPack(builder, flags);
  // add collection information
  CollectionAccessingNode::toVelocyPack(builder, flags);

  builder.add(VPackValue("variable"));
  _variable->toVelocyPack(builder);

  builder.add("attribute", VPackValue(VPackValueType::Array));
  for (auto const& it : _attribute) {
    builder.add(VPackValue(it));
  }
  builder.close();  // "attribute"

  if (!_satellites.empty()) {
    builder.add(VPackValue("satelliteCollections"));
    {
      VPackArrayBuilder guard(&builder);
      for (auto const& v : _satellites) {
        // if the mapped shard for a collection is empty, it means that
        // we have a vertex collection that is only relevant on some of the
        // target servers
        builder.add(VPackValue(v->name()));
      }
    }
  }
}

void DistributeNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  _variable = Variable::replace(_variable, replacements);
  _attribute = determineProjectionAttribute();
}

/// @brief getVariablesUsedHere, modifying the set in-place
void DistributeNode::getVariablesUsedHere(VarSet& vars) const {
  vars.emplace(_variable);
}

/// @brief estimateCost
CostEstimate DistributeNode::estimateCost() const {
  CostEstimate estimate = _dependencies[0]->getCost();
  estimate.estimatedCost += estimate.estimatedNrItems;
  return estimate;
}

void DistributeNode::addSatellite(aql::Collection* satellite) {
  // Only relevant for enterprise disjoint smart graphs
  TRI_ASSERT(satellite->isSatellite());
  _satellites.emplace_back(satellite);
}

size_t DistributeNode::getMemoryUsedBytes() const { return sizeof(*this); }
