////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionPlans
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionNode.h"
#include "Aql/Ast.h"
#include "Aql/ClusterNodes.h"
#include "Aql/Collection.h"
#include "Aql/CollectNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/SortNode.h"
#include "Aql/TraversalNode.h"
#include "Aql/ShortestPathNode.h"
#include "Aql/WalkerWorker.h"

using namespace arangodb::basics;
using namespace arangodb::aql;

static bool const Optional = true;

/// @brief maximum register id that can be assigned.
/// this is used for assertions
RegisterId const ExecutionNode::MaxRegisterId = 1000;

/// @brief type names
std::unordered_map<int, std::string const> const ExecutionNode::TypeNames{
    {static_cast<int>(ILLEGAL), "ExecutionNode (abstract)"},
    {static_cast<int>(SINGLETON), "SingletonNode"},
    {static_cast<int>(ENUMERATE_COLLECTION), "EnumerateCollectionNode"},
    {static_cast<int>(ENUMERATE_LIST), "EnumerateListNode"},
    {static_cast<int>(INDEX), "IndexNode"},
    {static_cast<int>(LIMIT), "LimitNode"},
    {static_cast<int>(CALCULATION), "CalculationNode"},
    {static_cast<int>(SUBQUERY), "SubqueryNode"},
    {static_cast<int>(FILTER), "FilterNode"},
    {static_cast<int>(SORT), "SortNode"},
    {static_cast<int>(COLLECT), "CollectNode"},
    {static_cast<int>(RETURN), "ReturnNode"},
    {static_cast<int>(REMOVE), "RemoveNode"},
    {static_cast<int>(INSERT), "InsertNode"},
    {static_cast<int>(UPDATE), "UpdateNode"},
    {static_cast<int>(REPLACE), "ReplaceNode"},
    {static_cast<int>(REMOTE), "RemoteNode"},
    {static_cast<int>(SCATTER), "ScatterNode"},
    {static_cast<int>(DISTRIBUTE), "DistributeNode"},
    {static_cast<int>(GATHER), "GatherNode"},
    {static_cast<int>(NORESULTS), "NoResultsNode"},
    {static_cast<int>(UPSERT), "UpsertNode"},
    {static_cast<int>(TRAVERSAL), "TraversalNode"},
    {static_cast<int>(SHORTEST_PATH), "ShortestPathNode"}};

/// @brief returns the type name of the node
std::string const& ExecutionNode::getTypeString() const {
  auto it = TypeNames.find(static_cast<int>(getType()));

  if (it != TypeNames.end()) {
    return (*it).second;
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                 "missing type in TypeNames");
}

void ExecutionNode::validateType(int type) {
  auto it = TypeNames.find(static_cast<int>(type));

  if (it == TypeNames.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "unknown TypeID");
  }
}

void ExecutionNode::getSortElements(SortElementVector& elements,
                                    ExecutionPlan* plan,
                                    arangodb::basics::Json const& oneNode,
                                    char const* which) {
  arangodb::basics::Json jsonElements = oneNode.get("elements");

  if (!jsonElements.isArray()) {
    std::string error = std::string("unexpected value for ") +
                        std::string(which) + std::string(" elements");
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, error);
  }

  size_t len = jsonElements.size();
  elements.reserve(len);

  for (size_t i = 0; i < len; i++) {
    arangodb::basics::Json oneJsonElement =
        jsonElements.at(static_cast<int>(i));
    bool ascending =
        JsonHelper::checkAndGetBooleanValue(oneJsonElement.json(), "ascending");
    Variable* v = varFromJson(plan->getAst(), oneJsonElement, "inVariable");
    elements.emplace_back(v, ascending);
  }
}

ExecutionNode* ExecutionNode::fromJsonFactory(
    ExecutionPlan* plan, arangodb::basics::Json const& oneNode) {
  auto JsonString = oneNode.toString();

  int nodeTypeID =
      JsonHelper::checkAndGetNumericValue<int>(oneNode.json(), "typeID");
  validateType(nodeTypeID);

  NodeType nodeType = (NodeType)nodeTypeID;

  switch (nodeType) {
    case SINGLETON:
      return new SingletonNode(plan, oneNode);
    case ENUMERATE_COLLECTION:
      return new EnumerateCollectionNode(plan, oneNode);
    case ENUMERATE_LIST:
      return new EnumerateListNode(plan, oneNode);
    case FILTER:
      return new FilterNode(plan, oneNode);
    case LIMIT:
      return new LimitNode(plan, oneNode);
    case CALCULATION:
      return new CalculationNode(plan, oneNode);
    case SUBQUERY:
      return new SubqueryNode(plan, oneNode);
    case SORT: {
      SortElementVector elements;
      bool stable =
          JsonHelper::checkAndGetBooleanValue(oneNode.json(), "stable");
      getSortElements(elements, plan, oneNode, "SortNode");
      return new SortNode(plan, oneNode, elements, stable);
    }
    case COLLECT: {
      Variable* expressionVariable =
          varFromJson(plan->getAst(), oneNode, "expressionVariable", Optional);
      Variable* outVariable =
          varFromJson(plan->getAst(), oneNode, "outVariable", Optional);

      arangodb::basics::Json jsonGroups = oneNode.get("groups");
      if (!jsonGroups.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                       "invalid groups definition");
      }

      arangodb::basics::Json jsonAggregates = oneNode.get("aggregates");
      if (!jsonAggregates.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                       "invalid aggregates definition");
      }

      std::vector<Variable const*> keepVariables;
      arangodb::basics::Json jsonKeepVariables = oneNode.get("keepVariables");
      if (jsonKeepVariables.isArray()) {
        size_t const n = jsonKeepVariables.size();
        for (size_t i = 0; i < n; i++) {
          arangodb::basics::Json keepVariable =
              jsonKeepVariables.at(static_cast<int>(i));
          Variable const* variable =
              varFromJson(plan->getAst(), keepVariable, "variable");
          keepVariables.emplace_back(variable);
        }
      }

      std::vector<std::pair<Variable const*, Variable const*>> groupVariables;
      {
        size_t const len = jsonGroups.size();
        groupVariables.reserve(len);
        for (size_t i = 0; i < len; i++) {
          arangodb::basics::Json oneJsonGroup =
              jsonGroups.at(static_cast<int>(i));
          Variable* outVar =
              varFromJson(plan->getAst(), oneJsonGroup, "outVariable");
          Variable* inVar =
              varFromJson(plan->getAst(), oneJsonGroup, "inVariable");

          groupVariables.emplace_back(std::make_pair(outVar, inVar));
        }
      }

      std::vector<
          std::pair<Variable const*, std::pair<Variable const*, std::string>>>
          aggregateVariables;
      {
        size_t const len = jsonAggregates.size();
        aggregateVariables.reserve(len);
        for (size_t i = 0; i < len; i++) {
          arangodb::basics::Json oneJsonAggregate =
              jsonAggregates.at(static_cast<int>(i));
          Variable* outVar =
              varFromJson(plan->getAst(), oneJsonAggregate, "outVariable");
          Variable* inVar =
              varFromJson(plan->getAst(), oneJsonAggregate, "inVariable");

          std::string const type = JsonHelper::checkAndGetStringValue(
              oneJsonAggregate.json(), "type");
          aggregateVariables.emplace_back(
              std::make_pair(outVar, std::make_pair(inVar, type)));
        }
      }

      bool count = JsonHelper::checkAndGetBooleanValue(oneNode.json(), "count");
      bool isDistinctCommand = JsonHelper::checkAndGetBooleanValue(
          oneNode.json(), "isDistinctCommand");

      auto node = new CollectNode(
          plan, oneNode, expressionVariable, outVariable, keepVariables,
          plan->getAst()->variables()->variables(false), groupVariables,
          aggregateVariables, count, isDistinctCommand);

      // specialize the node if required
      bool specialized =
          JsonHelper::checkAndGetBooleanValue(oneNode.json(), "specialized");
      if (specialized) {
        node->specialized();
      }

      return node;
    }
    case INSERT:
      return new InsertNode(plan, oneNode);
    case REMOVE:
      return new RemoveNode(plan, oneNode);
    case UPDATE:
      return new UpdateNode(plan, oneNode);
    case REPLACE:
      return new ReplaceNode(plan, oneNode);
    case UPSERT:
      return new UpsertNode(plan, oneNode);
    case RETURN:
      return new ReturnNode(plan, oneNode);
    case NORESULTS:
      return new NoResultsNode(plan, oneNode);
    case INDEX:
      return new IndexNode(plan, oneNode);
    case REMOTE:
      return new RemoteNode(plan, oneNode);
    case GATHER: {
      SortElementVector elements;
      getSortElements(elements, plan, oneNode, "GatherNode");
      return new GatherNode(plan, oneNode, elements);
    }
    case SCATTER:
      return new ScatterNode(plan, oneNode);
    case DISTRIBUTE:
      return new DistributeNode(plan, oneNode);
    case TRAVERSAL:
      return new TraversalNode(plan, oneNode);
    case SHORTEST_PATH:
      return new ShortestPathNode(plan, oneNode);
    case ILLEGAL: {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid node type");
    }
  }
  return nullptr;
}

/// @brief create an ExecutionNode from JSON
ExecutionNode::ExecutionNode(ExecutionPlan* plan,
                             arangodb::basics::Json const& json)
    : _id(JsonHelper::checkAndGetNumericValue<size_t>(json.json(), "id")),
      _estimatedCost(0.0),
      _estimatedCostSet(false),
      _depth(JsonHelper::checkAndGetNumericValue<int>(json.json(), "depth")),
      _varUsageValid(true),
      _plan(plan) {
  TRI_ASSERT(_registerPlan.get() == nullptr);
  _registerPlan.reset(new RegisterPlan());
  _registerPlan->clear();
  _registerPlan->depth = _depth;
  _registerPlan->totalNrRegs =
      JsonHelper::checkAndGetNumericValue<unsigned int>(json.json(),
                                                        "totalNrRegs");

  auto jsonVarInfoList = json.get("varInfoList");
  if (!jsonVarInfoList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "varInfoList needs to be a json array");
  }

  size_t len = jsonVarInfoList.size();
  _registerPlan->varInfo.reserve(len);

  for (size_t i = 0; i < len; i++) {
    auto jsonVarInfo = jsonVarInfoList.at(i);
    if (!jsonVarInfo.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "one varInfoList item needs to be a json object");
    }
    VariableId variableId = JsonHelper::checkAndGetNumericValue<VariableId>(
        jsonVarInfo.json(), "VariableId");
    RegisterId registerId = JsonHelper::checkAndGetNumericValue<RegisterId>(
        jsonVarInfo.json(), "RegisterId");
    unsigned int depth = JsonHelper::checkAndGetNumericValue<unsigned int>(
        jsonVarInfo.json(), "depth");

    _registerPlan->varInfo.emplace(variableId, VarInfo(depth, registerId));
  }

  auto jsonNrRegsList = json.get("nrRegs");
  if (!jsonNrRegsList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "nrRegs needs to be a json array");
  }

  len = jsonNrRegsList.size();
  _registerPlan->nrRegs.reserve(len);
  for (size_t i = 0; i < len; i++) {
    RegisterId oneReg =
        JsonHelper::getNumericValue<RegisterId>(jsonNrRegsList.at(i).json(), 0);
    _registerPlan->nrRegs.emplace_back(oneReg);
  }

  auto jsonNrRegsHereList = json.get("nrRegsHere");
  if (!jsonNrRegsHereList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "nrRegsHere needs to be a json array");
  }

  len = jsonNrRegsHereList.size();
  _registerPlan->nrRegsHere.reserve(len);
  for (size_t i = 0; i < len; i++) {
    RegisterId oneReg = JsonHelper::getNumericValue<RegisterId>(
        jsonNrRegsHereList.at(i).json(), 0);
    _registerPlan->nrRegsHere.emplace_back(oneReg);
  }

  auto jsonRegsToClearList = json.get("regsToClear");
  if (!jsonRegsToClearList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "regsToClear needs to be a json array");
  }

  len = jsonRegsToClearList.size();
  _regsToClear.reserve(len);
  for (size_t i = 0; i < len; i++) {
    RegisterId oneRegToClear = JsonHelper::getNumericValue<RegisterId>(
        jsonRegsToClearList.at(i).json(), 0);
    _regsToClear.emplace(oneRegToClear);
  }

  auto allVars = plan->getAst()->variables();

  auto jsonvarsUsedLater = json.get("varsUsedLater");
  if (!jsonvarsUsedLater.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "varsUsedLater needs to be a json array");
  }

  len = jsonvarsUsedLater.size();
  _varsUsedLater.reserve(len);
  for (size_t i = 0; i < len; i++) {
    // TODO: still Json Version. Ignore return val
    auto builder = JsonHelper::toVelocyPack(jsonvarsUsedLater.at(i).json());
    auto oneVarUsedLater = std::make_unique<Variable>(builder->slice());
    Variable* oneVariable = allVars->getVariable(oneVarUsedLater->id);

    if (oneVariable == nullptr) {
      std::string errmsg = "varsUsedLater: ID not found in all-array: " +
                           StringUtils::itoa(oneVarUsedLater->id);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, errmsg);
    }
    _varsUsedLater.emplace(oneVariable);
  }

  auto jsonvarsValidList = json.get("varsValid");

  if (!jsonvarsValidList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "varsValid needs to be a json array");
  }

  len = jsonvarsValidList.size();
  _varsValid.reserve(len);
  for (size_t i = 0; i < len; i++) {
    // TODO: deprecated
    auto builder = JsonHelper::toVelocyPack(jsonvarsValidList.at(i).json());
    auto oneVarValid = std::make_unique<Variable>(builder->slice());
    Variable* oneVariable = allVars->getVariable(oneVarValid->id);

    if (oneVariable == nullptr) {
      std::string errmsg = "varsValid: ID not found in all-array: " +
                           StringUtils::itoa(oneVarValid->id);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, errmsg);
    }
    _varsValid.emplace(oneVariable);
  }
}

/// @brief toVelocyPack, export an ExecutionNode to VelocyPack
void ExecutionNode::toVelocyPack(VPackBuilder& builder, 
                                 bool verbose, bool keepTopLevelOpen) const {
  // default value is to NOT keep top level open
  builder.openObject();
  builder.add(VPackValue("nodes"));
  {
    VPackArrayBuilder guard(&builder);
    toVelocyPackHelper(builder, verbose);
  }
  if (!keepTopLevelOpen) {
    builder.close();
  }
}

/// @brief execution Node clone utility to be called by derives
void ExecutionNode::cloneHelper(ExecutionNode* other, ExecutionPlan* plan,
                                bool withDependencies,
                                bool withProperties) const {
  plan->registerNode(other);

  if (withProperties) {
    other->_regsToClear = _regsToClear;
    other->_depth = _depth;
    other->_varUsageValid = _varUsageValid;

    auto allVars = plan->getAst()->variables();
    // Create new structures on the new AST...
    other->_varsUsedLater.reserve(_varsUsedLater.size());
    for (auto const& orgVar : _varsUsedLater) {
      auto var = allVars->getVariable(orgVar->id);
      TRI_ASSERT(var != nullptr);
      other->_varsUsedLater.emplace(var);
    }

    other->_varsValid.reserve(_varsValid.size());

    for (auto const& orgVar : _varsValid) {
      auto var = allVars->getVariable(orgVar->id);
      TRI_ASSERT(var != nullptr);
      other->_varsValid.emplace(var);
    }

    if (_registerPlan.get() != nullptr) {
      auto otherRegisterPlan =
          std::shared_ptr<RegisterPlan>(_registerPlan->clone(plan, _plan));
      other->_registerPlan = otherRegisterPlan;
    }
  } else {
    // point to current AST -> don't do deep copies.
    other->_depth = _depth;
    other->_regsToClear = _regsToClear;
    other->_varUsageValid = _varUsageValid;
    other->_varsUsedLater = _varsUsedLater;
    other->_varsValid = _varsValid;
    other->_registerPlan = _registerPlan;
  }

  if (withDependencies) {
    cloneDependencies(plan, other, withProperties);
  }
}

/// @brief helper for cloning, use virtual clone methods for dependencies
void ExecutionNode::cloneDependencies(ExecutionPlan* plan,
                                      ExecutionNode* theClone,
                                      bool withProperties) const {
  auto it = _dependencies.begin();
  while (it != _dependencies.end()) {
    auto c = (*it)->clone(plan, true, withProperties);
    try {
      c->_parents.emplace_back(theClone);
      theClone->_dependencies.emplace_back(c);
    } catch (...) {
      delete c;
      throw;
    }
    ++it;
  }
}

/// @brief convert to a string, basically for debugging purposes
void ExecutionNode::appendAsString(std::string& st, int indent) {
  for (int i = 0; i < indent; i++) {
    st.push_back(' ');
  }

  st.push_back('<');
  st.append(getTypeString());
  if (_dependencies.size() != 0) {
    st.push_back('\n');
    for (size_t i = 0; i < _dependencies.size(); i++) {
      _dependencies[i]->appendAsString(st, indent + 2);
      if (i != _dependencies.size() - 1) {
        st.push_back(',');
      } else {
        st.push_back(' ');
      }
    }
  }
  st.push_back('>');
}

/// @brief invalidate the cost estimation for the node and its dependencies
void ExecutionNode::invalidateCost() {
  _estimatedCostSet = false;

  for (auto& dep : _dependencies) {
    dep->invalidateCost();

    // no need to virtualize this function too, as getType(), estimateCost()
    // etc. are already virtual
    if (dep->getType() == SUBQUERY) {
      // invalid cost of subqueries, too
      static_cast<SubqueryNode*>(dep)->getSubquery()->invalidateCost();
    }
  }
}

/// @brief functionality to walk an execution plan recursively
bool ExecutionNode::walk(WalkerWorker<ExecutionNode>* worker) {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // Only do every node exactly once
  // note: this check is not required normally because execution
  // plans do not contain cycles
  if (worker->done(this)) {
    return false;
  }
#endif

  if (worker->before(this)) {
    return true;
  }

  // Now the children in their natural order:
  for (auto const& it : _dependencies) {
    if (it->walk(worker)) {
      return true;
    }
  }

  // Now handle a subquery:
  if (getType() == SUBQUERY) {
    auto p = static_cast<SubqueryNode*>(this);
    auto subquery = p->getSubquery();

    if (worker->enterSubquery(this, subquery)) {
      bool shouldAbort = subquery->walk(worker);
      worker->leaveSubquery(this, subquery);

      if (shouldAbort) {
        return true;
      }
    }
  }

  worker->after(this);

  return false;
}

/// @brief get the surrounding loop
ExecutionNode const* ExecutionNode::getLoop() const {
  auto node = this;
  while (node != nullptr) {
    if (!node->hasDependency()) {
      return nullptr;
    }

    node = node->getFirstDependency();
    TRI_ASSERT(node != nullptr);

    auto type = node->getType();

    if (type == ENUMERATE_COLLECTION || type == INDEX || type == TRAVERSAL ||
        type == ENUMERATE_LIST) {
      return node;
    }
  }

  return nullptr;
}

/// @brief factory for (optional) variables from json
Variable* ExecutionNode::varFromJson(Ast* ast,
                                     arangodb::basics::Json const& base,
                                     char const* variableName, bool optional) {
  arangodb::basics::Json variableJson = base.get(variableName);

  if (variableJson.isEmpty()) {
    if (optional) {
      return nullptr;
    }

    std::string msg;
    msg +=
        "Mandatory variable \"" + std::string(variableName) + "\" not found.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg.c_str());
  }
  // TODO: deprecated
  auto builder = JsonHelper::toVelocyPack(variableJson.json());
  return ast->variables()->createVariable(builder->slice());
}

/// @brief toVelocyPackHelper, for a generic node
/// Note: The input nodes has to be an Array Element that is still Open.
///       At the end of this function the current-nodes Object is OPEN and
///       has to be closed. The initial caller of toVelocyPackHelper
///       has to close the array.
void ExecutionNode::toVelocyPackHelperGeneric(VPackBuilder& nodes,
                                              bool verbose) const {
  TRI_ASSERT(nodes.isOpenArray());
  size_t const n = _dependencies.size();
  for (size_t i = 0; i < n; i++) {
    _dependencies[i]->toVelocyPackHelper(nodes, verbose);
  }
  nodes.openObject();
  nodes.add("type", VPackValue(getTypeString()));
  if (verbose) {
    nodes.add("typeID", VPackValue(static_cast<int>(getType())));
  }
  nodes.add(VPackValue("dependencies")); // Open Key
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& it : _dependencies) {
      nodes.add(VPackValue(static_cast<double>(it->id())));
    }
  }
  if (verbose) {
    nodes.add(VPackValue("parents")); // Open Key
    VPackArrayBuilder guard(&nodes);
    for (auto const& it : _parents) {
      nodes.add(VPackValue(static_cast<double>(it->id())));
    }
  }
  nodes.add("id", VPackValue(static_cast<double>(id())));
  size_t nrItems = 0;
  nodes.add("estimatedCost", VPackValue(getCost(nrItems)));
  nodes.add("estimatedNrItems", VPackValue(nrItems));

  if (verbose) {
    nodes.add("depth", VPackValue(static_cast<double>(_depth)));

    if (_registerPlan) {
      nodes.add(VPackValue("varInfoList"));
      {
        VPackArrayBuilder guard(&nodes);
        for (auto const& oneVarInfo : _registerPlan->varInfo) {
          VPackObjectBuilder guardInner(&nodes);
          nodes.add("VariableId",
                    VPackValue(static_cast<double>(oneVarInfo.first)));
          nodes.add("depth",
                    VPackValue(static_cast<double>(oneVarInfo.second.depth)));
          nodes.add(
              "RegisterId",
              VPackValue(static_cast<double>(oneVarInfo.second.registerId)));
        }
      }
      nodes.add(VPackValue("nrRegs"));
      {
        VPackArrayBuilder guard(&nodes);
        for (auto const& oneRegisterID : _registerPlan->nrRegs) {
          nodes.add(VPackValue(static_cast<double>(oneRegisterID)));
        }
      }
      nodes.add(VPackValue("nrRegsHere"));
      {
        VPackArrayBuilder guard(&nodes);
        for (auto const& oneRegisterID : _registerPlan->nrRegsHere) {
          nodes.add(VPackValue(static_cast<double>(oneRegisterID)));
        }
      }
      nodes.add("totalNrRegs", VPackValue(_registerPlan->totalNrRegs));
    } else {
      nodes.add(VPackValue("varInfoList"));
      {
        VPackArrayBuilder guard(&nodes);
      }
      nodes.add(VPackValue("nrRegs"));
      {
        VPackArrayBuilder guard(&nodes);
      }
      nodes.add(VPackValue("nrRegsHere"));
      {
        VPackArrayBuilder guard(&nodes);
      }
      nodes.add("totalNrRegs", VPackValue(0));
    }

    nodes.add(VPackValue("regsToClear"));
    {
      VPackArrayBuilder guard(&nodes);
      for (auto const& oneRegisterID : _regsToClear) {
        nodes.add(VPackValue(static_cast<double>(oneRegisterID)));
      }
    }

    nodes.add(VPackValue("varsUsedLater"));
    {
      VPackArrayBuilder guard(&nodes);
      for (auto const& oneVar : _varsUsedLater) {
        oneVar->toVelocyPack(nodes);
      }
    }

    nodes.add(VPackValue("varsValid"));
    {
      VPackArrayBuilder guard(&nodes);
      for (auto const& oneVar : _varsValid) {
        oneVar->toVelocyPack(nodes);
      }
    }
  }
  TRI_ASSERT(nodes.isOpenObject());
}

/// @brief static analysis debugger
#if 0
struct RegisterPlanningDebugger final : public WalkerWorker<ExecutionNode> {
  RegisterPlanningDebugger () 
    : indent(0) {
  }

  ~RegisterPlanningDebugger () {
  }

  int indent;

  bool enterSubquery (ExecutionNode*, ExecutionNode*) override final {
    indent++;
    return true;
  }

  void leaveSubquery (ExecutionNode*, ExecutionNode*) override final {
    indent--;
  }

  void after (ExecutionNode* ep) override final {
    for (int i = 0; i < indent; i++) {
      std::cout << " ";
    }
    std::cout << ep->getTypeString() << " ";
    std::cout << "regsUsedHere: ";
    for (auto const& v : ep->getVariablesUsedHere()) {
      std::cout << ep->getRegisterPlan()->varInfo.find(v->id)->second.registerId
                << " ";
    }
    std::cout << "regsSetHere: ";
    for (auto const& v : ep->getVariablesSetHere()) {
      std::cout << ep->getRegisterPlan()->varInfo.find(v->id)->second.registerId
                << " ";
    }
    std::cout << "regsToClear: ";
    for (auto const& r : ep->getRegsToClear()) {
      std::cout << r << " ";
    }
    std::cout << std::endl;
  }
};

#endif

/// @brief planRegisters
void ExecutionNode::planRegisters(ExecutionNode* super) {
  // The super is only for the case of subqueries.
  std::shared_ptr<RegisterPlan> v;

  if (super == nullptr) {
    v.reset(new RegisterPlan());
  } else {
    v.reset(new RegisterPlan(*(super->_registerPlan), super->_depth));
  }
  v->setSharedPtr(&v);

  walk(v.get());
  // Now handle the subqueries:
  for (auto& s : v->subQueryNodes) {
    auto sq = static_cast<SubqueryNode*>(s);
    sq->getSubquery()->planRegisters(s);
  }
  v->reset();

  // Just for debugging:
  /*
  std::cout << std::endl;
  RegisterPlanningDebugger debugger;
  walk(&debugger);
  std::cout << std::endl;
  */
}

// Copy constructor used for a subquery:
ExecutionNode::RegisterPlan::RegisterPlan(RegisterPlan const& v,
                                          unsigned int newdepth)
    : varInfo(v.varInfo),
      nrRegsHere(v.nrRegsHere),
      nrRegs(v.nrRegs),
      subQueryNodes(),
      depth(newdepth + 1),
      totalNrRegs(v.nrRegs[newdepth]),
      me(nullptr) {
  nrRegs.resize(depth);
  nrRegsHere.resize(depth);
  nrRegsHere.emplace_back(0);
  // create a copy of the last value here
  // this is requried because back returns a reference and emplace/push_back may
  // invalidate all references
  RegisterId registerId = nrRegs.back();
  nrRegs.emplace_back(registerId);
}

void ExecutionNode::RegisterPlan::clear() {
  varInfo.clear();
  nrRegsHere.clear();
  nrRegs.clear();
  subQueryNodes.clear();
  depth = 0;
  totalNrRegs = 0;
}

ExecutionNode::RegisterPlan* ExecutionNode::RegisterPlan::clone(
    ExecutionPlan* otherPlan, ExecutionPlan* plan) {
  auto other = std::make_unique<RegisterPlan>();

  other->nrRegsHere = nrRegsHere;
  other->nrRegs = nrRegs;
  other->depth = depth;
  other->totalNrRegs = totalNrRegs;

  other->varInfo = varInfo;

  // No need to clone subQueryNodes because this was only used during
  // the buildup.

  return other.release();
}

void ExecutionNode::RegisterPlan::after(ExecutionNode* en) {
  switch (en->getType()) {
    case ExecutionNode::ENUMERATE_COLLECTION: {
      depth++;
      nrRegsHere.emplace_back(1);
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = 1 + nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = static_cast<EnumerateCollectionNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      varInfo.emplace(ep->outVariable()->id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      break;
    }

    case ExecutionNode::INDEX: {
      depth++;
      nrRegsHere.emplace_back(1);
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = 1 + nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = static_cast<IndexNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      varInfo.emplace(ep->outVariable()->id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      break;
    }

    case ExecutionNode::ENUMERATE_LIST: {
      depth++;
      nrRegsHere.emplace_back(1);
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = 1 + nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = static_cast<EnumerateListNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      varInfo.emplace(ep->_outVariable->id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      break;
    }

    case ExecutionNode::CALCULATION: {
      nrRegsHere[depth]++;
      nrRegs[depth]++;
      auto ep = static_cast<CalculationNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      varInfo.emplace(ep->_outVariable->id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      break;
    }

    case ExecutionNode::SUBQUERY: {
      nrRegsHere[depth]++;
      nrRegs[depth]++;
      auto ep = static_cast<SubqueryNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      varInfo.emplace(ep->_outVariable->id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      subQueryNodes.emplace_back(en);
      break;
    }

    case ExecutionNode::COLLECT: {
      depth++;
      nrRegsHere.emplace_back(0);
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = static_cast<CollectNode const*>(en);
      for (auto const& p : ep->_groupVariables) {
        // p is std::pair<Variable const*,Variable const*>
        // and the first is the to be assigned output variable
        // for which we need to create a register in the current
        // frame:
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(p.first->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      for (auto const& p : ep->_aggregateVariables) {
        // p is std::pair<Variable const*,Variable const*>
        // and the first is the to be assigned output variable
        // for which we need to create a register in the current
        // frame:
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(p.first->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      if (ep->_outVariable != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->_outVariable->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::SORT: {
      // sort sorts in place and does not produce new registers
      break;
    }

    case ExecutionNode::RETURN: {
      // return is special. it produces a result but is the last step in the
      // pipeline
      break;
    }

    case ExecutionNode::REMOVE: {
      depth++;
      nrRegsHere.emplace_back(0);
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = static_cast<RemoveNode const*>(en);
      if (ep->getOutVariableOld() != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->getOutVariableOld()->id,
                        VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::INSERT: {
      depth++;
      nrRegsHere.emplace_back(0);
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = static_cast<InsertNode const*>(en);
      if (ep->getOutVariableNew() != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->getOutVariableNew()->id,
                        VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::UPDATE: {
      depth++;
      nrRegsHere.emplace_back(0);
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = static_cast<UpdateNode const*>(en);
      if (ep->getOutVariableOld() != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->getOutVariableOld()->id,
                        VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      if (ep->getOutVariableNew() != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->getOutVariableNew()->id,
                        VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::REPLACE: {
      depth++;
      nrRegsHere.emplace_back(0);
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      // when from the same underyling object (at least it does in Visual Studio
      // 2013)
      RegisterId registerId = nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = static_cast<ReplaceNode const*>(en);
      if (ep->getOutVariableOld() != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->getOutVariableOld()->id,
                        VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      if (ep->getOutVariableNew() != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->getOutVariableNew()->id,
                        VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::UPSERT: {
      depth++;
      nrRegsHere.emplace_back(0);
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = static_cast<UpsertNode const*>(en);
      if (ep->getOutVariableNew() != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->getOutVariableNew()->id,
                        VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::SINGLETON:
    case ExecutionNode::FILTER:
    case ExecutionNode::LIMIT:
    case ExecutionNode::SCATTER:
    case ExecutionNode::DISTRIBUTE:
    case ExecutionNode::GATHER:
    case ExecutionNode::REMOTE:
    case ExecutionNode::NORESULTS: {
      // these node types do not produce any new registers
      break;
    }

    case ExecutionNode::TRAVERSAL: {
      depth++;
      auto ep = static_cast<TraversalNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      auto vars = ep->getVariablesSetHere();
      nrRegsHere.emplace_back(static_cast<RegisterId>(vars.size()));
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId =
          static_cast<RegisterId>(vars.size() + nrRegs.back());
      nrRegs.emplace_back(registerId);

      for (auto& it : vars) {
        varInfo.emplace(it->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }
    case ExecutionNode::SHORTEST_PATH: {
      depth++;
      auto ep = static_cast<ShortestPathNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      auto vars = ep->getVariablesSetHere();
      nrRegsHere.emplace_back(static_cast<RegisterId>(vars.size()));
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId =
          static_cast<RegisterId>(vars.size() + nrRegs.back());
      nrRegs.emplace_back(registerId);

      for (auto& it : vars) {
        varInfo.emplace(it->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::ILLEGAL: {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                     "node type not implemented");
    }
  }

  en->_depth = depth;
  en->_registerPlan = *me;

  // Now find out which registers ought to be erased after this node:
  if (en->getType() != ExecutionNode::RETURN) {
    // ReturnNodes are special, since they return a single column anyway
    std::unordered_set<Variable const*> const& varsUsedLater =
        en->getVarsUsedLater();
    std::vector<Variable const*> const& varsUsedHere =
        en->getVariablesUsedHere();

    // We need to delete those variables that have been used here but are not
    // used any more later:
    std::unordered_set<RegisterId> regsToClear;

    for (auto const& v : varsUsedHere) {
      auto it = varsUsedLater.find(v);

      if (it == varsUsedLater.end()) {
        auto it2 = varInfo.find(v->id);

        if (it2 == varInfo.end()) {
          // report an error here to prevent crashing
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "missing variable #" + std::to_string(v->id) + " while planning registers"); 
        }

        // finally adjust the variable inside the IN calculation
        TRI_ASSERT(it2 != varInfo.end());
        RegisterId r = it2->second.registerId;
        regsToClear.emplace(r);
      }
    }
    en->setRegsToClear(regsToClear);
  }
}

/// @brief toJson, for SingletonNode
SingletonNode::SingletonNode(ExecutionPlan* plan,
                             arangodb::basics::Json const& base)
    : ExecutionNode(plan, base) {}

/// @brief toVelocyPack, for SingletonNode
void SingletonNode::toVelocyPackHelper(VPackBuilder& nodes,
                                       bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method
  // This node has no own information.
  nodes.close();
}

/// @brief the cost of a singleton is 1, it produces one item only
double SingletonNode::estimateCost(size_t& nrItems) const {
  nrItems = 1;
  return 1.0;
}

EnumerateCollectionNode::EnumerateCollectionNode(
    ExecutionPlan* plan, arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          JsonHelper::checkAndGetStringValue(base.json(), "collection"))),
      _outVariable(varFromJson(plan->getAst(), base, "outVariable")),
      _random(JsonHelper::checkAndGetBooleanValue(base.json(), "random")) {}

/// @brief toVelocyPack, for EnumerateCollectionNode
void EnumerateCollectionNode::toVelocyPackHelper(VPackBuilder& nodes,
                                                 bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

  // Now put info about vocbase and cid in there
  nodes.add("database", VPackValue(_vocbase->_name));
  nodes.add("collection", VPackValue(_collection->getName()));
  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);
  nodes.add("random", VPackValue(_random));

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
ExecutionNode* EnumerateCollectionNode::clone(ExecutionPlan* plan,
                                              bool withDependencies,
                                              bool withProperties) const {
  auto outVariable = _outVariable;
  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
    TRI_ASSERT(outVariable != nullptr);
  }

  auto c = new EnumerateCollectionNode(plan, _id, _vocbase, _collection,
                                       outVariable, _random);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

/// @brief the cost of an enumerate collection node is a multiple of the cost of
/// its unique dependency
double EnumerateCollectionNode::estimateCost(size_t& nrItems) const {
  size_t incoming;
  double depCost = _dependencies.at(0)->getCost(incoming);
  size_t count = _collection->count();
  nrItems = incoming * count;
  // We do a full collection scan for each incoming item.
  // random iteration is slightly more expensive than linear iteration
  return depCost + nrItems * (_random ? 1.005 : 1.0);
}

EnumerateListNode::EnumerateListNode(ExecutionPlan* plan,
                                     arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _inVariable(varFromJson(plan->getAst(), base, "inVariable")),
      _outVariable(varFromJson(plan->getAst(), base, "outVariable")) {}

/// @brief toVelocyPack, for EnumerateListNode
void EnumerateListNode::toVelocyPackHelper(VPackBuilder& nodes,
                                           bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method
  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
ExecutionNode* EnumerateListNode::clone(ExecutionPlan* plan,
                                        bool withDependencies,
                                        bool withProperties) const {
  auto outVariable = _outVariable;
  auto inVariable = _inVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
    inVariable = plan->getAst()->variables()->createVariable(inVariable);
  }

  auto c = new EnumerateListNode(plan, _id, inVariable, outVariable);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

/// @brief the cost of an enumerate list node
double EnumerateListNode::estimateCost(size_t& nrItems) const {
  size_t incoming = 0;
  double depCost = _dependencies.at(0)->getCost(incoming);

  // Well, what can we say? The length of the list can in general
  // only be determined at runtime... If we were to know that this
  // list is constant, then we could maybe multiply by the length
  // here... For the time being, we assume 100
  size_t length = 100;

  auto setter = _plan->getVarSetBy(_inVariable->id);

  if (setter != nullptr) {
    if (setter->getType() == ExecutionNode::CALCULATION) {
      // list variable introduced by a calculation
      auto expression = static_cast<CalculationNode*>(setter)->expression();

      if (expression != nullptr) {
        auto node = expression->node();

        if (node->type == NODE_TYPE_ARRAY) {
          // this one is easy
          length = node->numMembers();
        }
        if (node->type == NODE_TYPE_RANGE) {
          auto low = node->getMember(0);
          auto high = node->getMember(1);

          if (low->isConstant() && high->isConstant() &&
              (low->isValueType(VALUE_TYPE_INT) ||
               low->isValueType(VALUE_TYPE_DOUBLE)) &&
              (high->isValueType(VALUE_TYPE_INT) ||
               high->isValueType(VALUE_TYPE_DOUBLE))) {
            // create a temporary range to determine the size
            Range range(low->getIntValue(), high->getIntValue());

            length = range.size();
          }
        }
      }
    } else if (setter->getType() == ExecutionNode::SUBQUERY) {
      // length will be set by the subquery's cost estimator
      static_cast<SubqueryNode const*>(setter)->getSubquery()->estimateCost(
          length);
    }
  }

  nrItems = length * incoming;
  return depCost + static_cast<double>(length) * incoming;
}

LimitNode::LimitNode(ExecutionPlan* plan, arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _offset(JsonHelper::checkAndGetNumericValue<decltype(_offset)>(
          base.json(), "offset")),
      _limit(JsonHelper::checkAndGetNumericValue<decltype(_limit)>(base.json(),
                                                                   "limit")),
      _fullCount(
          JsonHelper::checkAndGetBooleanValue(base.json(), "fullCount")) {}

// @brief toVelocyPack, for LimitNode
void LimitNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, verbose);  // call base class method
  nodes.add("offset", VPackValue(static_cast<double>(_offset)));
  nodes.add("limit", VPackValue(static_cast<double>(_limit)));
  nodes.add("fullCount", VPackValue(_fullCount));

  // And close it:
  nodes.close();
}

/// @brief estimateCost
double LimitNode::estimateCost(size_t& nrItems) const {
  size_t incoming = 0;
  double depCost = _dependencies.at(0)->getCost(incoming);
  nrItems = (std::min)(_limit,
                       (std::max)(static_cast<size_t>(0), incoming - _offset));

  return depCost + nrItems;
}

CalculationNode::CalculationNode(ExecutionPlan* plan,
                                 arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _conditionVariable(
          varFromJson(plan->getAst(), base, "conditionVariable", true)),
      _outVariable(varFromJson(plan->getAst(), base, "outVariable")),
      _expression(new Expression(plan->getAst(), base)),
      _canRemoveIfThrows(false) {}

/// @brief toVelocyPack, for CalculationNode
void CalculationNode::toVelocyPackHelper(VPackBuilder& nodes,
                                         bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method
  nodes.add(VPackValue("expression"));
  _expression->toVelocyPack(nodes, verbose);

  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.add("canThrow", VPackValue(_expression->canThrow()));

  if (_conditionVariable != nullptr) {
    nodes.add(VPackValue("conditionVariable"));
    _conditionVariable->toVelocyPack(nodes);
  }

  nodes.add("expressionType", VPackValue(_expression->typeString()));

  // And close it
  nodes.close();
}

ExecutionNode* CalculationNode::clone(ExecutionPlan* plan,
                                      bool withDependencies,
                                      bool withProperties) const {
  auto conditionVariable = _conditionVariable;
  auto outVariable = _outVariable;

  if (withProperties) {
    if (_conditionVariable != nullptr) {
      conditionVariable =
          plan->getAst()->variables()->createVariable(conditionVariable);
    }
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }

  auto c = new CalculationNode(plan, _id, _expression->clone(),
                               conditionVariable, outVariable);
  c->_canRemoveIfThrows = _canRemoveIfThrows;

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

/// @brief estimateCost
double CalculationNode::estimateCost(size_t& nrItems) const {
  TRI_ASSERT(!_dependencies.empty());
  double depCost = _dependencies.at(0)->getCost(nrItems);
  return depCost + nrItems;
}

SubqueryNode::SubqueryNode(ExecutionPlan* plan,
                           arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _subquery(nullptr),
      _outVariable(varFromJson(plan->getAst(), base, "outVariable")) {}

/// @brief toVelocyPack, for SubqueryNode
void SubqueryNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

  nodes.add(VPackValue("subquery"));
  _subquery->toVelocyPack(nodes, verbose);
  nodes.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(nodes);

  nodes.add("isConst", VPackValue(const_cast<SubqueryNode*>(this)->isConst()));

  // And add it:
  nodes.close();
}
  
bool SubqueryNode::isConst() {
  if (isModificationQuery() || !isDeterministic()) {
    return false;
  }

  for (auto const& v : getVariablesUsedHere()) {
    auto setter = _plan->getVarSetBy(v->id);

    if (setter == nullptr || setter->getType() != CALCULATION) {
      return false;
    }

    auto expression = static_cast<CalculationNode const*>(setter)->expression();

    if (expression == nullptr) {
      return false;
    }
    if (!expression->isConstant()) {
      return false;
    }
  }
      
  return true;
}

ExecutionNode* SubqueryNode::clone(ExecutionPlan* plan, bool withDependencies,
                                   bool withProperties) const {
  auto outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }
  auto c = new SubqueryNode(
      plan, _id, _subquery->clone(plan, true, withProperties), outVariable);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

/// @brief whether or not the subquery is a data-modification operation
bool SubqueryNode::isModificationQuery() const {
  std::vector<ExecutionNode*> stack({_subquery});

  while (!stack.empty()) {
    ExecutionNode* current = stack.back();

    if (current->isModificationNode()) {
      return true;
    }
    
    stack.pop_back();

    current->addDependencies(stack);
  }

  return false;
}

/// @brief replace the out variable, so we can adjust the name.
void SubqueryNode::replaceOutVariable(Variable const* var) {
  _outVariable = var;
}

/// @brief estimateCost
double SubqueryNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies.at(0)->getCost(nrItems);
  size_t nrItemsSubquery;
  double subCost = _subquery->getCost(nrItemsSubquery);
  return depCost + nrItems * subCost;
}

/// @brief helper struct to find all (outer) variables used in a SubqueryNode
struct SubqueryVarUsageFinder final : public WalkerWorker<ExecutionNode> {
  std::unordered_set<Variable const*> _usedLater;
  std::unordered_set<Variable const*> _valid;

  SubqueryVarUsageFinder() {}

  ~SubqueryVarUsageFinder() {}

  bool before(ExecutionNode* en) override final {
    // Add variables used here to _usedLater:
    for (auto const& v : en->getVariablesUsedHere()) {
      _usedLater.emplace(v);
    }
    return false;
  }

  void after(ExecutionNode* en) override final {
    // Add variables set here to _valid:
    for (auto& v : en->getVariablesSetHere()) {
      _valid.emplace(v);
    }
  }

  bool enterSubquery(ExecutionNode*, ExecutionNode* sub) override final {
    SubqueryVarUsageFinder subfinder;
    sub->walk(&subfinder);

    // keep track of all variables used by a (dependent) subquery
    // this is, all variables in the subqueries _usedLater that are not in
    // _valid

    // create the set difference. note: cannot use std::set_difference as our
    // sets are NOT sorted
    for (auto it = subfinder._usedLater.begin();
         it != subfinder._usedLater.end(); ++it) {
      if (_valid.find(*it) != _valid.end()) {
        _usedLater.emplace(*it);
      }
    }
    return false;
  }
};

/// @brief getVariablesUsedHere, returning a vector
std::vector<Variable const*> SubqueryNode::getVariablesUsedHere() const {
  SubqueryVarUsageFinder finder;
  _subquery->walk(&finder);

  std::vector<Variable const*> v;
  for (auto it = finder._usedLater.begin(); it != finder._usedLater.end();
       ++it) {
    if (finder._valid.find(*it) == finder._valid.end()) {
      v.emplace_back(*it);
    }
  }

  return v;
}

/// @brief getVariablesUsedHere, modifying the set in-place
void SubqueryNode::getVariablesUsedHere(
    std::unordered_set<Variable const*>& vars) const {
  SubqueryVarUsageFinder finder;
  _subquery->walk(&finder);

  for (auto it = finder._usedLater.begin(); it != finder._usedLater.end();
       ++it) {
    if (finder._valid.find(*it) == finder._valid.end()) {
      vars.emplace(*it);
    }
  }
}

/// @brief can the node throw? We have to find whether any node in the
/// subquery plan can throw.
struct CanThrowFinder final : public WalkerWorker<ExecutionNode> {
  bool _canThrow;

  CanThrowFinder() : _canThrow(false) {}
  ~CanThrowFinder() = default;

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* node) override final {
    if (node->canThrow()) {
      _canThrow = true;
      return true;
    }
    return false;
  }
};

/// @brief is the node determistic?
struct IsDeterministicFinder final : public WalkerWorker<ExecutionNode> {
  bool _isDeterministic = true;

  IsDeterministicFinder() : _isDeterministic(true) {}
  ~IsDeterministicFinder() {}

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;
  }

  bool before(ExecutionNode* node) override final {
    if (!node->isDeterministic()) {
      _isDeterministic = false;
      return true;
    }
    return false;
  }
};

bool SubqueryNode::canThrow() {
  CanThrowFinder finder;
  _subquery->walk(&finder);
  return finder._canThrow;
}

bool SubqueryNode::isDeterministic() {
  IsDeterministicFinder finder;
  _subquery->walk(&finder);
  return finder._isDeterministic;
}

FilterNode::FilterNode(ExecutionPlan* plan, arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _inVariable(varFromJson(plan->getAst(), base, "inVariable")) {}

/// @brief toVelocyPack, for FilterNode
void FilterNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

ExecutionNode* FilterNode::clone(ExecutionPlan* plan, bool withDependencies,
                                 bool withProperties) const {
  auto inVariable = _inVariable;

  if (withProperties) {
    inVariable = plan->getAst()->variables()->createVariable(inVariable);
  }
  auto c = new FilterNode(plan, _id, inVariable);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

/// @brief estimateCost
double FilterNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies.at(0)->getCost(nrItems);
  // We are pessimistic here by not reducing the nrItems. However, in the
  // worst case the filter does not reduce the items at all. Furthermore,
  // no optimizer rule introduces FilterNodes, thus it is not important
  // that they appear to lower the costs. Note that contrary to this,
  // an IndexNode does lower the costs, it also has a better idea
  // to what extent the number of items is reduced. On the other hand it
  // is important that a FilterNode produces additional costs, otherwise
  // the rule throwing away a FilterNode that is already covered by an
  // IndexNode cannot reduce the costs.
  return depCost + nrItems;
}

ReturnNode::ReturnNode(ExecutionPlan* plan, arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _inVariable(varFromJson(plan->getAst(), base, "inVariable")) {}

/// @brief toVelocyPack, for ReturnNode
void ReturnNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method


  nodes.add(VPackValue("inVariable"));
  _inVariable->toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
ExecutionNode* ReturnNode::clone(ExecutionPlan* plan, bool withDependencies,
                                 bool withProperties) const {
  auto inVariable = _inVariable;

  if (withProperties) {
    inVariable = plan->getAst()->variables()->createVariable(inVariable);
  }

  auto c = new ReturnNode(plan, _id, inVariable);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

/// @brief estimateCost
double ReturnNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies.at(0)->getCost(nrItems);
  return depCost + nrItems;
}

/// @brief toVelocyPack, for NoResultsNode
void NoResultsNode::toVelocyPackHelper(VPackBuilder& nodes,
                                       bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, verbose);

  //And close it
  nodes.close();
}

/// @brief estimateCost, the cost of a NoResults is nearly 0
double NoResultsNode::estimateCost(size_t& nrItems) const {
  nrItems = 0;
  return 0.5;  // just to make it non-zero
}

