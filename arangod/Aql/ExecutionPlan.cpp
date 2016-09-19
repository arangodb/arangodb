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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionPlan.h"
#include "Aql/CollectOptions.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/CollectNode.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/ModificationNodes.h"
#include "Aql/NodeFinder.h"
#include "Aql/Optimizer.h"
#include "Aql/Query.h"
#include "Aql/ShortestPathNode.h"
#include "Aql/ShortestPathOptions.h"
#include "Aql/SortNode.h"
#include "Aql/TraversalNode.h"
#include "Aql/Variable.h"
#include "Aql/WalkerWorker.h"
#include "Basics/Exceptions.h"
#include "Basics/SmallVector.h"
#include "Basics/StaticStrings.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

static uint64_t checkTraversalDepthValue(AstNode const* node) {
  if (!node->isNumericValue()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                   "invalid traversal depth");
  }
  double v = node->getDoubleValue();
  double intpart;
  if (modf(v, &intpart) != 0.0 || v < 0.0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                   "invalid traversal depth");
  }
  return static_cast<uint64_t>(v);
}

static std::unique_ptr<traverser::TraverserOptions> CreateTraversalOptions(
    Transaction* trx, AstNode const* direction, AstNode const* optionsNode) {

  auto options = std::make_unique<traverser::TraverserOptions>(trx);

  TRI_ASSERT(direction != nullptr);
  TRI_ASSERT(direction->type == NODE_TYPE_DIRECTION);
  TRI_ASSERT(direction->numMembers() == 2);

  auto steps = direction->getMember(1);

  if (steps->isNumericValue()) {
    // Check if a double value is integer
    options->minDepth = checkTraversalDepthValue(steps);
    options->maxDepth = options->minDepth;
  } else if (steps->type == NODE_TYPE_RANGE) {
    // Range depth
    options->minDepth = checkTraversalDepthValue(steps->getMember(0));
    options->maxDepth = checkTraversalDepthValue(steps->getMember(1));

    if (options->maxDepth < options->minDepth) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                     "invalid traversal depth");
    }
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                   "invalid traversal depth");
  }

  if (optionsNode != nullptr && optionsNode->type == NODE_TYPE_OBJECT) {
    size_t n = optionsNode->numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = optionsNode->getMember(i);

      if (member != nullptr && member->type == NODE_TYPE_OBJECT_ELEMENT) {
        std::string const name = member->getString();
        auto value = member->getMember(0);

        TRI_ASSERT(value->isConstant());

        if (name == "bfs") {
          options->useBreadthFirst = value->isTrue();
        } else if (name == "uniqueVertices" && value->isStringValue()) {
          if (value->stringEquals("path", true)) {
            options->uniqueVertices =
                arangodb::traverser::TraverserOptions::UniquenessLevel::PATH;
          } else if (value->stringEquals("global", true)) {
            options->uniqueVertices =
                arangodb::traverser::TraverserOptions::UniquenessLevel::GLOBAL;
          }
        } else if (name == "uniqueEdges" && value->isStringValue()) {
          if (value->stringEquals("none", true)) {
            options->uniqueEdges =
                arangodb::traverser::TraverserOptions::UniquenessLevel::NONE;
          } else if (value->stringEquals("global", true)) {
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_BAD_PARAMETER,
                "uniqueEdges: 'global' is not supported, "
                "due to unpredictable results. Use 'path' "
                "or 'none' instead");
          }
        }
      }
    }
  }
  if (options->uniqueVertices ==
          arangodb::traverser::TraverserOptions::UniquenessLevel::GLOBAL &&
      !options->useBreadthFirst) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "uniqueVertices: 'global' is only "
                                   "supported, with bfs: true due to "
                                   "unpredictable results.");
  }
  return options;
}

static ShortestPathOptions CreateShortestPathOptions(AstNode const* node) {
  ShortestPathOptions options;

  if (node != nullptr && node->type == NODE_TYPE_OBJECT) {
    size_t n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = node->getMember(i);

      if (member != nullptr && member->type == NODE_TYPE_OBJECT_ELEMENT) {
        std::string const name = member->getString();
        auto value = member->getMember(0);

        TRI_ASSERT(value->isConstant());

        if (name == "weightAttribute" && value->isStringValue()) {
          options.weightAttribute =
              std::string(value->getStringValue(), value->getStringLength());
        } else if (name == "defaultWeight" && value->isNumericValue()) {
          options.defaultWeight = value->getDoubleValue();
        }
      }
    }
  }
  return options;
}




/// @brief create the plan
ExecutionPlan::ExecutionPlan(Ast* ast)
    : _ids(),
      _root(nullptr),
      _varUsageComputed(false),
      _nextId(0),
      _ast(ast),
      _lastLimitNode(nullptr),
      _subqueries() {}

/// @brief destroy the plan, frees all assigned nodes
ExecutionPlan::~ExecutionPlan() {
  for (auto& x : _ids) {
    delete x.second;
  }
}

/// @brief create an execution plan from an AST
ExecutionPlan* ExecutionPlan::instantiateFromAst(Ast* ast) {
  TRI_ASSERT(ast != nullptr);

  auto root = ast->root();
  TRI_ASSERT(root != nullptr);
  TRI_ASSERT(root->type == NODE_TYPE_ROOT);

  auto plan = std::make_unique<ExecutionPlan>(ast);

  plan->_root = plan->fromNode(root);

  // insert fullCount flag
  if (plan->_lastLimitNode != nullptr &&
      ast->query()->getBooleanOption("fullCount", false)) {
    static_cast<LimitNode*>(plan->_lastLimitNode)->setFullCount();
  }

  plan->findVarUsage();

  return plan.release();
}

/// @brief process the list of collections in a VelocyPack
void ExecutionPlan::getCollectionsFromVelocyPack(Ast* ast,
                                                 VPackSlice const slice) {
  TRI_ASSERT(ast != nullptr);

  VPackSlice collectionsSlice = slice.get("collections");

  if (!collectionsSlice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "json node \"collections\" not found or not an array");
  }

  for (auto const& collection : VPackArrayIterator(collectionsSlice)) {
    auto typeStr = arangodb::basics::VelocyPackHelper::checkAndGetStringValue(
        collection, "type");
    ast->query()->collections()->add(
        arangodb::basics::VelocyPackHelper::checkAndGetStringValue(collection,
                                                                   "name"),
        TRI_GetTransactionTypeFromStr(
            arangodb::basics::VelocyPackHelper::checkAndGetStringValue(
                collection, "type")
                .c_str()));
  }
}

/// @brief create an execution plan from VelocyPack
ExecutionPlan* ExecutionPlan::instantiateFromVelocyPack(
    Ast* ast, VPackSlice const slice) {
  TRI_ASSERT(ast != nullptr);

  auto plan = std::make_unique<ExecutionPlan>(ast);
  plan->_root = plan->fromSlice(slice);
  plan->_varUsageComputed = true;

  return plan.release();
}

/// @brief clone the plan by recursively cloning starting from the root
class CloneNodeAdder final : public WalkerWorker<ExecutionNode> {
  ExecutionPlan* _plan;

 public:
  bool success;

  explicit CloneNodeAdder(ExecutionPlan* plan) : _plan(plan), success(true) {}

  ~CloneNodeAdder() {}

  bool before(ExecutionNode* node) override final {
    // We need to catch exceptions because the walk has to finish
    // and either register the nodes or delete them.
    try {
      _plan->registerNode(node);
    } catch (...) {
      success = false;
    }
    return false;
  }
};

/// @brief clone an existing execution plan
ExecutionPlan* ExecutionPlan::clone() {
  auto plan = std::make_unique<ExecutionPlan>(_ast);

  plan->_root = _root->clone(plan.get(), true, false);
  plan->_nextId = _nextId;
  plan->_appliedRules = _appliedRules;

  CloneNodeAdder adder(plan.get());
  plan->_root->walk(&adder);

  if (!adder.success) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Could not clone plan");
  }
  // plan->findVarUsage();
  // Let's not do it here, because supposedly the plan is modified as
  // the very next thing anyway!

  return plan.release();
}

/// @brief create an execution plan identical to this one
///   keep the memory of the plan on the query object specified.
ExecutionPlan* ExecutionPlan::clone(Query const& query) {
  auto otherPlan = std::make_unique<ExecutionPlan>(query.ast());

  for (auto const& it : _ids) {
    otherPlan->registerNode(it.second->clone(otherPlan.get(), false, true));
  }

  return otherPlan.release();
}

/// @brief export to VelocyPack
std::shared_ptr<VPackBuilder> ExecutionPlan::toVelocyPack(Ast* ast, bool verbose) const {
  auto builder = std::make_shared<VPackBuilder>();

  toVelocyPack(*builder, ast, verbose);
  return builder;
}

/// @brief export to VelocyPack
void ExecutionPlan::toVelocyPack(VPackBuilder& builder, Ast* ast, bool verbose) const {
  // keeps top level of built object open
  _root->toVelocyPack(builder, verbose, true);

  TRI_ASSERT(!builder.isClosed());

  // set up rules
  builder.add(VPackValue("rules"));
  builder.openArray();
  for (auto const& r : Optimizer::translateRules(_appliedRules)) {
    builder.add(VPackValue(r));
  }
  builder.close();
  
  // set up collections
  builder.add(VPackValue("collections"));
  builder.openArray();
  for (auto const& c : *ast->query()->collections()->collections()) {
    builder.openObject();
    builder.add("name", VPackValue(c.first));
    builder.add("type",
                VPackValue(TRI_TransactionTypeGetStr(c.second->accessType)));
    builder.close();
  }
  builder.close();

  // set up variables
  builder.add(VPackValue("variables"));
  ast->variables()->toVelocyPack(builder);

  size_t nrItems = 0;
  builder.add("estimatedCost", VPackValue(_root->getCost(nrItems)));
  builder.add("estimatedNrItems", VPackValue(nrItems));

  builder.close();
}

/// @brief get a list of all applied rules
std::vector<std::string> ExecutionPlan::getAppliedRules() const {
  return Optimizer::translateRules(_appliedRules);
}

/// @brief get a node by its id
ExecutionNode* ExecutionPlan::getNodeById(size_t id) const {
  auto it = _ids.find(id);

  if (it != _ids.end()) {
    // node found
    return (*it).second;
  }

  std::string msg = std::string("node [") + std::to_string(id) +
                    std::string("] wasn't found");
  // node unknown
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
}

/// @brief creates a calculation node for an arbitrary expression
ExecutionNode* ExecutionPlan::createCalculation(
    Variable* out, Variable const* conditionVariable, AstNode const* expression,
    ExecutionNode* previous) {
  TRI_ASSERT(out != nullptr);

  bool const isDistinct = (expression->type == NODE_TYPE_DISTINCT);

  if (isDistinct) {
    TRI_ASSERT(expression->numMembers() == 1);
    expression = expression->getMember(0);
  }

  // generate a temporary calculation node
  auto expr =
      std::make_unique<Expression>(_ast, const_cast<AstNode*>(expression));

  CalculationNode* en;
  if (conditionVariable != nullptr) {
    en =
        new CalculationNode(this, nextId(), expr.get(), conditionVariable, out);
  } else {
    en = new CalculationNode(this, nextId(), expr.get(), out);
  }
  expr.release();

  registerNode(reinterpret_cast<ExecutionNode*>(en));

  if (previous != nullptr) {
    en->addDependency(previous);
  }

  if (!isDistinct) {
    return en;
  }

  // DISTINCT expression is implemented by creating an anonymous COLLECT node
  auto collectNode = createAnonymousCollect(en);

  TRI_ASSERT(en != nullptr);
  collectNode->addDependency(en);

  return collectNode;
}

/// @brief get the subquery node from an expression
/// this will return a nullptr if the expression does not refer to a subquery
SubqueryNode* ExecutionPlan::getSubqueryFromExpression(
    AstNode const* expression) const {
  TRI_ASSERT(expression != nullptr);

  if (expression->type != NODE_TYPE_REFERENCE) {
    return nullptr;
  }

  auto referencedVariable = static_cast<Variable const*>(expression->getData());

  TRI_ASSERT(referencedVariable != nullptr);

  if (referencedVariable->isUserDefined()) {
    return nullptr;
  }

  auto it = _subqueries.find(referencedVariable->id);

  if (it == _subqueries.end()) {
    return nullptr;
  }

  return static_cast<SubqueryNode*>((*it).second);
}

/// @brief get the output variable from a node
Variable const* ExecutionPlan::getOutVariable(ExecutionNode const* node) const {
  if (node->getType() == ExecutionNode::CALCULATION) {
    // CalculationNode has an outVariale() method
    return static_cast<CalculationNode const*>(node)->outVariable();
  }

  if (node->getType() == ExecutionNode::COLLECT) {
    // CollectNode has an outVariale() method, but we cannot use it.
    // for CollectNode, outVariable() will return the variable filled by INTO,
    // but INTO is an optional feature
    // so this will return the first result variable of the COLLECT
    // this part of the code should only be called for anonymous COLLECT nodes,
    // which only have one result variable
    auto en = static_cast<CollectNode const*>(node);
    auto const& vars = en->groupVariables();

    TRI_ASSERT(vars.size() == 1);
    auto v = vars[0].first;
    TRI_ASSERT(v != nullptr);
    return v;
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid node type in getOutVariable");
}

/// @brief creates an anonymous COLLECT node (for a DISTINCT)
CollectNode* ExecutionPlan::createAnonymousCollect(
    CalculationNode const* previous) {
  // generate an out variable for the COLLECT
  auto out = _ast->variables()->createTemporaryVariable();
  TRI_ASSERT(out != nullptr);

  std::vector<std::pair<Variable const*, Variable const*>> const groupVariables{
      std::make_pair(out, previous->outVariable())};
  std::vector<
      std::pair<Variable const*, std::pair<Variable const*, std::string>>> const
      aggregateVariables{};

  auto en = new CollectNode(this, nextId(), CollectOptions(), groupVariables,
                            aggregateVariables, nullptr, nullptr,
                            std::vector<Variable const*>(),
                            _ast->variables()->variables(false), false, true);

  registerNode(reinterpret_cast<ExecutionNode*>(en));

  return en;
}

/// @brief create modification options from an AST node
ModificationOptions ExecutionPlan::createModificationOptions(
    AstNode const* node) {
  ModificationOptions options;

  // parse the modification options we got
  if (node != nullptr && node->type == NODE_TYPE_OBJECT) {
    size_t n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = node->getMember(i);

      if (member != nullptr && member->type == NODE_TYPE_OBJECT_ELEMENT) {
        std::string const name = member->getString();
        auto value = member->getMember(0);

        TRI_ASSERT(value->isConstant());

        if (name == "waitForSync") {
          options.waitForSync = value->isTrue();
        } else if (name == "ignoreErrors") {
          options.ignoreErrors = value->isTrue();
        } else if (name == "keepNull") {
          // nullMeansRemove is the opposite of keepNull
          options.nullMeansRemove = value->isFalse();
        } else if (name == "mergeObjects") {
          options.mergeObjects = value->isTrue();
        }
      }
    }
  }

  // this means a data-modification query must first read the entire input data
  // before starting with the modifications
  // this is safe in all cases
  // the flag can be set to false later to execute read and write operations in
  // lockstep
  options.readCompleteInput = true;

  if (!_ast->functionsMayAccessDocuments()) {
    // no functions in the query can access document data...
    bool isReadWrite = false;

    if (_ast->containsTraversal()) {
      // its unclear which collections the traversal will access
      isReadWrite = true;
    } else {
      auto const collections = _ast->query()->collections();

      for (auto const& it : *(collections->collections())) {
        if (it.second->isReadWrite) {
          isReadWrite = true;
          break;
        }
      }
    }

    if (!isReadWrite) {
      // no collection is used in both read and write mode
      // this means the query's write operation can use read & write in lockstep
      options.readCompleteInput = false;
    }
  }

  return options;
}

/// @brief create COLLECT options from an AST node
CollectOptions ExecutionPlan::createCollectOptions(AstNode const* node) {
  CollectOptions options;

  // parse the modification options we got
  if (node != nullptr && node->type == NODE_TYPE_OBJECT) {
    size_t n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = node->getMember(i);

      if (member != nullptr && member->type == NODE_TYPE_OBJECT_ELEMENT) {
        std::string const name = member->getString();
        auto value = member->getMember(0);

        TRI_ASSERT(value->isConstant());

        if (name == "method") {
          if (value->isStringValue()) {
            options.method =
                CollectOptions::methodFromString(value->getString());
          }
        }
      }
    }
  }

  return options;
}

/// @brief register a node with the plan, will delete node if addition fails
ExecutionNode* ExecutionPlan::registerNode(ExecutionNode* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->id() > 0);

  try {
    _ids.emplace(node->id(), node);
  } catch (...) {
    delete node;
    throw;
  }
  return node;
}

/// @brief creates a anonymous calculation node for an arbitrary expression
ExecutionNode* ExecutionPlan::createTemporaryCalculation(
    AstNode const* expression, ExecutionNode* previous) {
  // generate a temporary variable
  auto out = _ast->variables()->createTemporaryVariable();
  TRI_ASSERT(out != nullptr);

  return createCalculation(out, nullptr, expression, previous);
}

/// @brief adds "previous" as dependency to "plan", returns "plan"
ExecutionNode* ExecutionPlan::addDependency(ExecutionNode* previous,
                                            ExecutionNode* plan) {
  TRI_ASSERT(previous != nullptr);
  TRI_ASSERT(plan != nullptr);

  try {
    plan->addDependency(previous);
    return plan;
  } catch (...) {
    // prevent memleak
    delete plan;
    throw;
  }
}

/// @brief create an execution plan element from an AST FOR node
ExecutionNode* ExecutionPlan::fromNodeFor(ExecutionNode* previous,
                                          AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_FOR);
  TRI_ASSERT(node->numMembers() == 2);

  auto variable = node->getMember(0);
  auto expression = node->getMember(1);

  // fetch 1st operand (out variable name)
  TRI_ASSERT(variable->type == NODE_TYPE_VARIABLE);
  auto v = static_cast<Variable*>(variable->getData());
  TRI_ASSERT(v != nullptr);

  ExecutionNode* en = nullptr;

  // peek at second operand
  if (expression->type == NODE_TYPE_COLLECTION) {
    // second operand is a collection
    std::string const collectionName = expression->getString();
    auto collections = _ast->query()->collections();
    auto collection = collections->get(collectionName);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "no collection for EnumerateCollection");
    }
    en = registerNode(new EnumerateCollectionNode(
        this, nextId(), _ast->query()->vocbase(), collection, v, false));
  } else if (expression->type == NODE_TYPE_REFERENCE) {
    // second operand is already a variable
    auto inVariable = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(inVariable != nullptr);
    en = registerNode(new EnumerateListNode(this, nextId(), inVariable, v));
  } else {
    // second operand is some misc. expression
    auto calc = createTemporaryCalculation(expression, previous);
    en = registerNode(
        new EnumerateListNode(this, nextId(), getOutVariable(calc), v));
    previous = calc;
  }

  TRI_ASSERT(en != nullptr);

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST FOR TRAVERSAL node
ExecutionNode* ExecutionPlan::fromNodeTraversal(ExecutionNode* previous,
                                                AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_TRAVERSAL);
  TRI_ASSERT(node->numMembers() >= 5);
  TRI_ASSERT(node->numMembers() <= 7);

  // the first 3 members are used by traversal internally.
  // The members 4-6, where 5 and 6 are optional, are used
  // as out variables.
  AstNode const* direction = node->getMember(0);
  AstNode const* start = node->getMember(1);
  AstNode const* graph = node->getMember(2);

  if (start->type == NODE_TYPE_OBJECT && start->isConstant()) {
    size_t n = start->numMembers();
    for (size_t i = 0; i < n; ++i) {
      auto member = start->getMember(i);
      if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
          member->getString() == StaticStrings::IdString) {
        start = member->getMember(0);
        break;
      }
    }
  }

  if (start->type != NODE_TYPE_REFERENCE && start->type != NODE_TYPE_VALUE) {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(start, previous);
    start = _ast->createNodeReference(getOutVariable(calc));
    previous = calc;
  }

  auto options = CreateTraversalOptions(getAst()->query()->trx(), direction,
                                        node->getMember(3));

  // First create the node
  auto travNode = new TraversalNode(this, nextId(), _ast->query()->vocbase(),
                                    direction, start, graph, options);

  auto variable = node->getMember(4);
  TRI_ASSERT(variable->type == NODE_TYPE_VARIABLE);
  auto v = static_cast<Variable*>(variable->getData());
  TRI_ASSERT(v != nullptr);
  travNode->setVertexOutput(v);

  if (node->numMembers() > 5) {
    // return the edge as well
    variable = node->getMember(5);
    TRI_ASSERT(variable->type == NODE_TYPE_VARIABLE);
    v = static_cast<Variable*>(variable->getData());
    TRI_ASSERT(v != nullptr);
    travNode->setEdgeOutput(v);
    if (node->numMembers() > 6) {
      // return the path as well
      variable = node->getMember(6);
      TRI_ASSERT(variable->type == NODE_TYPE_VARIABLE);
      v = static_cast<Variable*>(variable->getData());
      TRI_ASSERT(v != nullptr);
      travNode->setPathOutput(v);
    }
  }

  ExecutionNode* en = registerNode(travNode);
  TRI_ASSERT(en != nullptr);
  return addDependency(previous, en);
}

AstNode const* ExecutionPlan::parseTraversalVertexNode(ExecutionNode*& previous,
                                                       AstNode const* vertex) {
  if (vertex->type == NODE_TYPE_OBJECT && vertex->isConstant()) {
    size_t n = vertex->numMembers();
    for (size_t i = 0; i < n; ++i) {
      auto member = vertex->getMember(i);
      if (member->type == NODE_TYPE_OBJECT_ELEMENT &&
          member->getString() == StaticStrings::IdString) {
        vertex = member->getMember(0);
        break;
      }
    }
  }

  if (vertex->type != NODE_TYPE_REFERENCE && vertex->type != NODE_TYPE_VALUE) {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(vertex, previous);
    vertex = _ast->createNodeReference(getOutVariable(calc));
    // update previous so the caller has an updated value
    previous = calc;
  }

  return vertex;
}

/// @brief create an execution plan element from an AST for SHORTEST_PATH node
ExecutionNode* ExecutionPlan::fromNodeShortestPath(ExecutionNode* previous,
                                                   AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_SHORTEST_PATH);
  TRI_ASSERT(node->numMembers() >= 6);
  TRI_ASSERT(node->numMembers() <= 7);

  // the first 4 members are used by shortest_path internally.
  // The members 5-6, where 6 is optional, are used
  // as out variables.
  AstNode const* direction = node->getMember(0);
  TRI_ASSERT(direction->isIntValue());
  AstNode const* start = parseTraversalVertexNode(previous, node->getMember(1));
  AstNode const* target = parseTraversalVertexNode(previous, node->getMember(2));
  AstNode const* graph = node->getMember(3);

  ShortestPathOptions options = CreateShortestPathOptions(node->getMember(4));


  // First create the node
  auto spNode = new ShortestPathNode(this, nextId(), _ast->query()->vocbase(),
                                     direction->getIntValue(), start, target,
                                     graph, options);

  auto variable = node->getMember(5);
  TRI_ASSERT(variable->type == NODE_TYPE_VARIABLE);
  auto v = static_cast<Variable*>(variable->getData());
  TRI_ASSERT(v != nullptr);
  spNode->setVertexOutput(v);

  if (node->numMembers() > 6) {
    // return the edge as well
    variable = node->getMember(6);
    TRI_ASSERT(variable->type == NODE_TYPE_VARIABLE);
    v = static_cast<Variable*>(variable->getData());
    TRI_ASSERT(v != nullptr);
    spNode->setEdgeOutput(v);
  }

  ExecutionNode* en = registerNode(spNode);
  TRI_ASSERT(en != nullptr);
  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST FILTER node
ExecutionNode* ExecutionPlan::fromNodeFilter(ExecutionNode* previous,
                                             AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_FILTER);
  TRI_ASSERT(node->numMembers() == 1);

  auto expression = node->getMember(0);

  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = registerNode(new FilterNode(this, nextId(), v));
  } else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(expression, previous);
    en = registerNode(new FilterNode(this, nextId(), getOutVariable(calc)));
    previous = calc;
  }

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST LET node
/// this also includes handling of subqueries (as subqueries can only occur
/// inside LET nodes)
ExecutionNode* ExecutionPlan::fromNodeLet(ExecutionNode* previous,
                                          AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_LET);
  TRI_ASSERT(node->numMembers() >= 2);

  AstNode const* variable = node->getMember(0);
  AstNode const* expression = node->getMember(1);

  Variable const* conditionVariable = nullptr;

  if (node->numMembers() > 2) {
    // a LET with an IF condition
    auto condition = createTemporaryCalculation(node->getMember(2), previous);
    previous = condition;
    conditionVariable = getOutVariable(condition);
  }

  auto v = static_cast<Variable*>(variable->getData());

  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_SUBQUERY) {
    // operand is a subquery...
    auto subquery = fromNode(expression);

    if (subquery == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    en = registerNode(new SubqueryNode(this, nextId(), subquery, v));
    _subqueries[static_cast<SubqueryNode*>(en)->outVariable()->id] = en;
  } else {
    // check if the LET is a reference to a subquery
    auto subquery = getSubqueryFromExpression(expression);

    if (subquery != nullptr) {
      // optimization: if the LET a = ... references a variable created by a
      // subquery,
      // change the output variable of the (anonymous) subquery to be the
      // outvariable of
      // the LET. and don't create the LET

      subquery->replaceOutVariable(v);
      return subquery;
    }
    // otherwise fall-through to normal behavior

    // operand is some misc expression, potentially including references to
    // other variables
    return createCalculation(v, conditionVariable, expression, previous);
  }

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST SORT node
ExecutionNode* ExecutionPlan::fromNodeSort(ExecutionNode* previous,
                                           AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_SORT);
  TRI_ASSERT(node->numMembers() == 1);

  auto list = node->getMember(0);
  TRI_ASSERT(list->type == NODE_TYPE_ARRAY);

  std::vector<std::pair<Variable const*, bool>> elements;
  std::vector<ExecutionNode*> temp;

  try {
    size_t const n = list->numMembers();
    elements.reserve(n);

    for (size_t i = 0; i < n; ++i) {
      auto element = list->getMember(i);
      TRI_ASSERT(element != nullptr);
      TRI_ASSERT(element->type == NODE_TYPE_SORT_ELEMENT);
      TRI_ASSERT(element->numMembers() == 2);

      auto expression = element->getMember(0);
      auto ascending = element->getMember(1);

      // get sort order
      bool isAscending;
      bool handled = false;
      if (ascending->type == NODE_TYPE_VALUE) {
        if (ascending->value.type == VALUE_TYPE_STRING) {
          // special treatment for string values ASC/DESC
          if (ascending->stringEquals("ASC", true)) {
            isAscending = true;
            handled = true;
          } else if (ascending->stringEquals("DESC", true)) {
            isAscending = false;
            handled = true;
          }
        }
      }

      if (!handled) {
        // if no sort order is set, ensure we have one
        auto ascendingNode = ascending->castToBool(_ast);
        if (ascendingNode->type == NODE_TYPE_VALUE &&
            ascendingNode->value.type == VALUE_TYPE_BOOL) {
          isAscending = ascendingNode->value.value._bool;
        } else {
          // must have an order
          isAscending = true;
        }
      }

      if (expression->type == NODE_TYPE_REFERENCE) {
        // sort operand is a variable
        auto v = static_cast<Variable*>(expression->getData());
        TRI_ASSERT(v != nullptr);
        elements.emplace_back(std::make_pair(v, isAscending));
      } else {
        // sort operand is some misc expression
        auto calc = createTemporaryCalculation(expression, nullptr);
        temp.emplace_back(calc);
        elements.emplace_back(
            std::make_pair(getOutVariable(calc), isAscending));
      }
    }
  } catch (...) {
    // prevent memleak
    for (auto& it : temp) {
      delete it;
    }
    throw;
  }

  if (elements.empty()) {
    // no sort criterion remained - this can only happen if all sort
    // criteria were constants
    return previous;
  }

  // at least one sort criterion remained
  TRI_ASSERT(!elements.empty());

  // properly link the temporary calculations in the plan
  for (auto it = temp.begin(); it != temp.end(); ++it) {
    TRI_ASSERT(previous != nullptr);
    (*it)->addDependency(previous);
    previous = (*it);
  }

  auto en = registerNode(new SortNode(this, nextId(), elements, false));

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST COLLECT node
ExecutionNode* ExecutionPlan::fromNodeCollect(ExecutionNode* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_COLLECT);
  TRI_ASSERT(node->numMembers() == 6);

  auto options = createCollectOptions(node->getMember(0));

  auto groups = node->getMember(1);
  size_t const numVars = groups->numMembers();

  std::vector<std::pair<Variable const*, Variable const*>> groupVariables;
  groupVariables.reserve(numVars);

  for (size_t i = 0; i < numVars; ++i) {
    auto assigner = groups->getMember(i);

    if (assigner == nullptr) {
      continue;
    }

    TRI_ASSERT(assigner->type == NODE_TYPE_ASSIGN);
    auto out = assigner->getMember(0);
    TRI_ASSERT(out != nullptr);
    auto v = static_cast<Variable*>(out->getData());
    TRI_ASSERT(v != nullptr);

    auto expression = assigner->getMember(1);

    if (expression->type == NODE_TYPE_REFERENCE) {
      // operand is a variable
      auto e = static_cast<Variable*>(expression->getData());
      groupVariables.emplace_back(std::make_pair(v, e));
    } else {
      // operand is some misc expression
      auto calc = createTemporaryCalculation(expression, previous);
      previous = calc;
      groupVariables.emplace_back(std::make_pair(v, getOutVariable(calc)));
    }
  }

  // aggregate variables
  std::vector<
      std::pair<Variable const*, std::pair<Variable const*, std::string>>>
      aggregateVariables;
  {
    auto list = node->getMember(2);
    TRI_ASSERT(list->type == NODE_TYPE_AGGREGATIONS);
    list = list->getMember(0);
    TRI_ASSERT(list->type == NODE_TYPE_ARRAY);
    size_t const numVars = list->numMembers();

    aggregateVariables.reserve(numVars);
    for (size_t i = 0; i < numVars; ++i) {
      auto assigner = list->getMember(i);

      if (assigner == nullptr) {
        continue;
      }

      TRI_ASSERT(assigner->type == NODE_TYPE_ASSIGN);
      auto out = assigner->getMember(0);
      TRI_ASSERT(out != nullptr);
      auto v = static_cast<Variable*>(out->getData());
      TRI_ASSERT(v != nullptr);

      auto expression = assigner->getMember(1);

      // operand is always a function call
      TRI_ASSERT(expression->type == NODE_TYPE_FCALL);

      // build aggregator
      auto func = static_cast<Function*>(expression->getData());
      TRI_ASSERT(func != nullptr);

      // function should have one argument (an array with the parameters)
      TRI_ASSERT(expression->numMembers() == 1);

      auto args = expression->getMember(0);
      // the number of arguments should also be one (note: this has been
      // validated before)
      TRI_ASSERT(args->type == NODE_TYPE_ARRAY);
      TRI_ASSERT(args->numMembers() == 1);

      auto arg = args->getMember(0);

      if (arg->type == NODE_TYPE_REFERENCE) {
        // operand is a variable
        auto e = static_cast<Variable*>(arg->getData());
        aggregateVariables.emplace_back(
            std::make_pair(v, std::make_pair(e, func->externalName)));
      } else {
        auto calc = createTemporaryCalculation(arg, previous);
        previous = calc;

        aggregateVariables.emplace_back(std::make_pair(
            v, std::make_pair(getOutVariable(calc), func->externalName)));
      }
    }
  }

  // handle out variable
  Variable const* outVariable = nullptr;
  auto const into = node->getMember(3);

  if (into->type != NODE_TYPE_NOP) {
    outVariable = static_cast<Variable const*>(into->getData());
  }

  Variable const* expressionVariable = nullptr;
  auto const expression = node->getMember(4);

  if (expression->type != NODE_TYPE_NOP) {
    if (expression->type == NODE_TYPE_REFERENCE) {
      // expression is already a variable
      auto variable = static_cast<Variable const*>(expression->getData());
      TRI_ASSERT(variable != nullptr);
      expressionVariable = variable;
    } else {
      // expression is some misc expression
      auto calc = createTemporaryCalculation(expression, previous);
      previous = calc;
      expressionVariable = getOutVariable(calc);
    }
  }

  std::vector<Variable const*> keepVariables;
  auto const keep = node->getMember(5);

  if (keep->type != NODE_TYPE_NOP) {
    // variables to keep
    TRI_ASSERT(keep->type == NODE_TYPE_ARRAY);
    size_t const keepVarsSize = keep->numMembers();
    keepVariables.reserve(keepVarsSize);

    for (size_t i = 0; i < keepVarsSize; ++i) {
      auto ref = keep->getMember(i);
      TRI_ASSERT(ref->type == NODE_TYPE_REFERENCE);
      keepVariables.emplace_back(static_cast<Variable const*>(ref->getData()));
    }
  }

  auto collectNode = new CollectNode(
      this, nextId(), options, groupVariables, aggregateVariables,
      expressionVariable, outVariable, keepVariables,
      _ast->variables()->variables(false), false, false);

  auto en = registerNode(collectNode);

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST COLLECT node, COUNT
/// note that also a sort plan node will be added in front of the collect plan
/// node
ExecutionNode* ExecutionPlan::fromNodeCollectCount(ExecutionNode* previous,
                                                   AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_COLLECT_COUNT);
  TRI_ASSERT(node->numMembers() == 3);

  auto options = createCollectOptions(node->getMember(0));

  auto list = node->getMember(1);
  size_t const numVars = list->numMembers();

  std::vector<std::pair<Variable const*, Variable const*>> groupVariables;
  groupVariables.reserve(numVars);
  for (size_t i = 0; i < numVars; ++i) {
    auto assigner = list->getMember(i);

    if (assigner == nullptr) {
      continue;
    }

    TRI_ASSERT(assigner->type == NODE_TYPE_ASSIGN);
    auto out = assigner->getMember(0);
    TRI_ASSERT(out != nullptr);
    auto v = static_cast<Variable*>(out->getData());
    TRI_ASSERT(v != nullptr);

    auto expression = assigner->getMember(1);

    if (expression->type == NODE_TYPE_REFERENCE) {
      // operand is a variable
      auto e = static_cast<Variable*>(expression->getData());
      groupVariables.emplace_back(std::make_pair(v, e));
    } else {
      // operand is some misc expression
      auto calc = createTemporaryCalculation(expression, previous);
      previous = calc;
      groupVariables.emplace_back(std::make_pair(v, getOutVariable(calc)));
    }
  }

  // output variable
  auto v = node->getMember(2);
  // handle out variable
  Variable* outVariable = static_cast<Variable*>(v->getData());

  TRI_ASSERT(outVariable != nullptr);

  std::vector<
      std::pair<Variable const*, std::pair<Variable const*, std::string>>> const
      aggregateVariables{};

  auto collectNode = new CollectNode(
      this, nextId(), options, groupVariables, aggregateVariables, nullptr,
      outVariable, std::vector<Variable const*>(),
      _ast->variables()->variables(false), true, false);
  auto en = registerNode(collectNode);

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST LIMIT node
ExecutionNode* ExecutionPlan::fromNodeLimit(ExecutionNode* previous,
                                            AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_LIMIT);
  TRI_ASSERT(node->numMembers() == 2);

  auto offset = node->getMember(0);
  auto count = node->getMember(1);

  TRI_ASSERT(offset->type == NODE_TYPE_VALUE);
  TRI_ASSERT(count->type == NODE_TYPE_VALUE);

  int64_t offsetValue = 0;
  if (offset->value.type != VALUE_TYPE_NULL) {
    if ((offset->value.type != VALUE_TYPE_INT &&
         offset->value.type != VALUE_TYPE_DOUBLE) ||
        offset->getIntValue() < 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE,
          "LIMIT offset value is not a number or out of range");
    }
    offsetValue = offset->getIntValue();
  }

  int64_t countValue =
      128 * 1024 *
      1024;  // arbitrary default value for an "unbounded" limit value
  if (count->value.type != VALUE_TYPE_NULL) {
    if ((count->value.type != VALUE_TYPE_INT &&
         count->value.type != VALUE_TYPE_DOUBLE) ||
        count->getIntValue() < 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE,
          "LIMIT count value is not a number or out of range");
    }
    countValue = count->getIntValue();
  }

  auto en = registerNode(new LimitNode(this, nextId(),
                                       static_cast<size_t>(offsetValue),
                                       static_cast<size_t>(countValue)));

  _lastLimitNode = en;

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST RETURN node
ExecutionNode* ExecutionPlan::fromNodeReturn(ExecutionNode* previous,
                                             AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_RETURN);
  TRI_ASSERT(node->numMembers() == 1);

  auto expression = node->getMember(0);

  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = registerNode(new ReturnNode(this, nextId(), v));
  } else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(expression, previous);
    en = registerNode(new ReturnNode(this, nextId(), getOutVariable(calc)));
    previous = calc;
  }

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST REMOVE node
ExecutionNode* ExecutionPlan::fromNodeRemove(ExecutionNode* previous,
                                             AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_REMOVE);
  TRI_ASSERT(node->numMembers() == 4);

  auto options = createModificationOptions(node->getMember(0));
  std::string const collectionName = node->getMember(1)->getString();
  auto collections = _ast->query()->collections();
  auto collection = collections->get(collectionName);

  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "no collection for RemoveNode");
  }

  auto expression = node->getMember(2);
  ExecutionNode* en = nullptr;

  auto returnVarNode = node->getMember(3);
  Variable const* outVariableOld =
      static_cast<Variable*>(returnVarNode->getData());

  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = registerNode(new RemoveNode(this, nextId(), _ast->query()->vocbase(),
                                     collection, options, v, outVariableOld));
  } else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(expression, previous);
    en = registerNode(new RemoveNode(this, nextId(), _ast->query()->vocbase(),
                                     collection, options, getOutVariable(calc),
                                     outVariableOld));
    previous = calc;
  }

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST INSERT node
ExecutionNode* ExecutionPlan::fromNodeInsert(ExecutionNode* previous,
                                             AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_INSERT);
  TRI_ASSERT(node->numMembers() == 4);

  auto options = createModificationOptions(node->getMember(0));
  std::string const collectionName = node->getMember(1)->getString();
  auto collections = _ast->query()->collections();
  auto collection = collections->get(collectionName);
  auto expression = node->getMember(2);

  auto returnVarNode = node->getMember(3);
  Variable const* outVariableNew =
      static_cast<Variable*>(returnVarNode->getData());

  ExecutionNode* en = nullptr;

  if (expression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(expression->getData());
    TRI_ASSERT(v != nullptr);
    en = registerNode(new InsertNode(this, nextId(), _ast->query()->vocbase(),
                                     collection, options, v, outVariableNew));
  } else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(expression, previous);
    en = registerNode(new InsertNode(this, nextId(), _ast->query()->vocbase(),
                                     collection, options, getOutVariable(calc),
                                     outVariableNew));
    previous = calc;
  }

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST UPDATE node
ExecutionNode* ExecutionPlan::fromNodeUpdate(ExecutionNode* previous,
                                             AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_UPDATE);
  TRI_ASSERT(node->numMembers() == 6);

  auto options = createModificationOptions(node->getMember(0));
  std::string const collectionName = node->getMember(1)->getString();
  auto collections = _ast->query()->collections();
  auto collection = collections->get(collectionName);
  auto docExpression = node->getMember(2);
  auto keyExpression = node->getMember(3);

  Variable const* keyVariable = nullptr;
  ExecutionNode* en = nullptr;

  Variable const* outVariableOld =
      static_cast<Variable*>(node->getMember(4)->getData());
  Variable const* outVariableNew =
      static_cast<Variable*>(node->getMember(5)->getData());

  if (keyExpression->type == NODE_TYPE_NOP) {
    keyExpression = nullptr;
  }

  if (keyExpression != nullptr) {
    if (keyExpression->type == NODE_TYPE_REFERENCE) {
      // key operand is already a variable
      auto v = static_cast<Variable*>(keyExpression->getData());
      TRI_ASSERT(v != nullptr);
      keyVariable = v;
    } else {
      // key operand is some misc expression
      auto calc = createTemporaryCalculation(keyExpression, previous);
      previous = calc;
      keyVariable = getOutVariable(calc);
    }
  }

  if (docExpression->type == NODE_TYPE_REFERENCE) {
    // document operand is already a variable
    auto v = static_cast<Variable*>(docExpression->getData());
    TRI_ASSERT(v != nullptr);
    en = registerNode(new UpdateNode(this, nextId(), _ast->query()->vocbase(),
                                     collection, options, v, keyVariable,
                                     outVariableOld, outVariableNew));
  } else {
    // document operand is some misc expression
    auto calc = createTemporaryCalculation(docExpression, previous);
    en = registerNode(new UpdateNode(
        this, nextId(), _ast->query()->vocbase(), collection, options,
        getOutVariable(calc), keyVariable, outVariableOld, outVariableNew));
    previous = calc;
  }

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST REPLACE node
ExecutionNode* ExecutionPlan::fromNodeReplace(ExecutionNode* previous,
                                              AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_REPLACE);
  TRI_ASSERT(node->numMembers() == 6);

  auto options = createModificationOptions(node->getMember(0));
  std::string const collectionName = node->getMember(1)->getString();
  auto collections = _ast->query()->collections();
  auto collection = collections->get(collectionName);
  auto docExpression = node->getMember(2);
  auto keyExpression = node->getMember(3);

  Variable const* keyVariable = nullptr;
  ExecutionNode* en = nullptr;

  Variable const* outVariableOld =
      static_cast<Variable*>(node->getMember(4)->getData());
  Variable const* outVariableNew =
      static_cast<Variable*>(node->getMember(5)->getData());

  if (keyExpression->type == NODE_TYPE_NOP) {
    keyExpression = nullptr;
  }

  if (keyExpression != nullptr) {
    if (keyExpression->type == NODE_TYPE_REFERENCE) {
      // key operand is already a variable
      auto v = static_cast<Variable*>(keyExpression->getData());
      TRI_ASSERT(v != nullptr);
      keyVariable = v;
    } else {
      // key operand is some misc expression
      auto calc = createTemporaryCalculation(keyExpression, previous);
      previous = calc;
      keyVariable = getOutVariable(calc);
    }
  }

  if (docExpression->type == NODE_TYPE_REFERENCE) {
    // operand is already a variable
    auto v = static_cast<Variable*>(docExpression->getData());
    TRI_ASSERT(v != nullptr);
    en = registerNode(new ReplaceNode(this, nextId(), _ast->query()->vocbase(),
                                      collection, options, v, keyVariable,
                                      outVariableOld, outVariableNew));
  } else {
    // operand is some misc expression
    auto calc = createTemporaryCalculation(docExpression, previous);
    en = registerNode(new ReplaceNode(
        this, nextId(), _ast->query()->vocbase(), collection, options,
        getOutVariable(calc), keyVariable, outVariableOld, outVariableNew));
    previous = calc;
  }

  return addDependency(previous, en);
}

/// @brief create an execution plan element from an AST UPSERT node
ExecutionNode* ExecutionPlan::fromNodeUpsert(ExecutionNode* previous,
                                             AstNode const* node) {
  TRI_ASSERT(node != nullptr && node->type == NODE_TYPE_UPSERT);
  TRI_ASSERT(node->numMembers() == 7);

  auto options = createModificationOptions(node->getMember(0));
  std::string const collectionName = node->getMember(1)->getString();
  auto collections = _ast->query()->collections();
  auto collection = collections->get(collectionName);
  auto docExpression = node->getMember(2);
  auto insertExpression = node->getMember(3);
  auto updateExpression = node->getMember(4);

  Variable const* outVariableNew =
      static_cast<Variable*>(node->getMember(6)->getData());

  TRI_ASSERT(docExpression->type == NODE_TYPE_REFERENCE);
  // doc operand is already a variable
  auto docVariable = static_cast<Variable*>(docExpression->getData());
  TRI_ASSERT(docVariable != nullptr);

  Variable const* insertVar;
  if (insertExpression->type == NODE_TYPE_REFERENCE) {
    // insert operand is already a variable
    insertVar = static_cast<Variable*>(insertExpression->getData());
  } else {
    auto calc = createTemporaryCalculation(insertExpression, previous);
    previous = calc;
    insertVar = getOutVariable(calc);
  }
  TRI_ASSERT(insertVar != nullptr);

  Variable const* updateVar;
  if (updateExpression->type == NODE_TYPE_REFERENCE) {
    // insert operand is already a variable
    updateVar = static_cast<Variable*>(updateExpression->getData());
  } else {
    auto calc = createTemporaryCalculation(updateExpression, previous);
    previous = calc;
    updateVar = getOutVariable(calc);
  }
  TRI_ASSERT(updateVar != nullptr);

  bool isReplace =
      (node->getIntValue(true) == static_cast<int64_t>(NODE_TYPE_REPLACE));

  ExecutionNode* en = registerNode(new UpsertNode(
      this, nextId(), _ast->query()->vocbase(), collection, options,
      docVariable, insertVar, updateVar, outVariableNew, isReplace));

  return addDependency(previous, en);
}

/// @brief create an execution plan from an abstract syntax tree node
ExecutionNode* ExecutionPlan::fromNode(AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  ExecutionNode* en = registerNode(new SingletonNode(this, nextId()));

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMember(i);

    if (member == nullptr || member->type == NODE_TYPE_NOP) {
      continue;
    }

    switch (member->type) {
      case NODE_TYPE_WITH: {
        // the using declaration...
        break;
      }

      case NODE_TYPE_FOR: {
        en = fromNodeFor(en, member);
        break;
      }

      case NODE_TYPE_TRAVERSAL: {
        en = fromNodeTraversal(en, member);
        break;
      }

      case NODE_TYPE_SHORTEST_PATH: {
        en = fromNodeShortestPath(en, member);
        break;
      }

      case NODE_TYPE_FILTER: {
        en = fromNodeFilter(en, member);
        break;
      }

      case NODE_TYPE_LET: {
        en = fromNodeLet(en, member);
        break;
      }

      case NODE_TYPE_SORT: {
        en = fromNodeSort(en, member);
        break;
      }

      case NODE_TYPE_COLLECT: {
        en = fromNodeCollect(en, member);
        break;
      }

      case NODE_TYPE_COLLECT_COUNT: {
        en = fromNodeCollectCount(en, member);
        break;
      }

      case NODE_TYPE_LIMIT: {
        en = fromNodeLimit(en, member);
        break;
      }

      case NODE_TYPE_RETURN: {
        en = fromNodeReturn(en, member);
        break;
      }

      case NODE_TYPE_REMOVE: {
        en = fromNodeRemove(en, member);
        break;
      }

      case NODE_TYPE_INSERT: {
        en = fromNodeInsert(en, member);
        break;
      }

      case NODE_TYPE_UPDATE: {
        en = fromNodeUpdate(en, member);
        break;
      }

      case NODE_TYPE_REPLACE: {
        en = fromNodeReplace(en, member);
        break;
      }

      case NODE_TYPE_UPSERT: {
        en = fromNodeUpsert(en, member);
        break;
      }

      default: {
        // node type not implemented
        en = nullptr;
        break;
      }
    }

    if (en == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "type not handled");
    }
  }

  return en;
}

/// @brief find nodes of a certain type
void ExecutionPlan::findNodesOfType(SmallVector<ExecutionNode*>& result,
                                    ExecutionNode::NodeType type,
                                    bool enterSubqueries) {
  NodeFinder<ExecutionNode::NodeType> finder(type, result, enterSubqueries);
  root()->walk(&finder);
}


/// @brief find nodes of a certain types
void ExecutionPlan::findNodesOfType(SmallVector<ExecutionNode*>& result,
    std::vector<ExecutionNode::NodeType> const& types, bool enterSubqueries) {
  NodeFinder<std::vector<ExecutionNode::NodeType>> finder(types, result,
                                                          enterSubqueries);
  root()->walk(&finder);
}

/// @brief find all end nodes in a plan
void ExecutionPlan::findEndNodes(SmallVector<ExecutionNode*>& result,
    bool enterSubqueries) const {
  EndNodeFinder finder(result, enterSubqueries);
  root()->walk(&finder);
}

/// @brief check linkage of execution plan
#if 0
class LinkChecker : public WalkerWorker<ExecutionNode> {

  public:
    LinkChecker () {
    }

    bool before (ExecutionNode* en) {
      auto deps = en->getDependencies();
      for (auto x : deps) {
        auto parents = x->getParents();
        bool ok = false;
        for (auto it = parents.begin(); it != parents.end(); ++it) {
          if (*it == en) {
            ok = true;
            break;
          }
        }
        if (! ok) {
          std::cout << "Found dependency which does not have us as a parent!"
                    << std::endl;
        }
      }
      auto parents = en->getParents();
      if (parents.size() > 1) {
        std::cout << "Found a node with more than one parent!" << std::endl;
      }
      for (auto x : parents) {
        auto deps = x->getDependencies();
        bool ok = false;
        for (auto it = deps.begin(); it != deps.end(); ++it) {
          if (*it == en) {
            ok = true;
            break;
          }
        }
        if (! ok) {
          std::cout << "Found parent which does not have us as a dependency!"
                    << std::endl;
        }
      }
      return false;
    }
};

void ExecutionPlan::checkLinkage () {
  LinkChecker checker;
  root()->walk(&checker);
}

#endif

/// @brief helper struct for findVarUsage
struct VarUsageFinder final : public WalkerWorker<ExecutionNode> {
  std::unordered_set<Variable const*> _usedLater;
  std::unordered_set<Variable const*> _valid;
  std::unordered_map<VariableId, ExecutionNode*>* _varSetBy;
  bool const _ownsVarSetBy;

  VarUsageFinder() : _varSetBy(nullptr), _ownsVarSetBy(true) {
    _varSetBy = new std::unordered_map<VariableId, ExecutionNode*>();
  }

  explicit VarUsageFinder(
      std::unordered_map<VariableId, ExecutionNode*>* varSetBy)
      : _varSetBy(varSetBy), _ownsVarSetBy(false) {
    TRI_ASSERT(_varSetBy != nullptr);
  }

  ~VarUsageFinder() {
    if (_ownsVarSetBy) {
      TRI_ASSERT(_varSetBy != nullptr);
      delete _varSetBy;
    }
  }

  bool before(ExecutionNode* en) override final {
    en->invalidateVarUsage();
    en->setVarsUsedLater(_usedLater);
    // Add variables used here to _usedLater:

    en->getVariablesUsedHere(_usedLater);

    return false;
  }

  void after(ExecutionNode* en) override final {
    // Add variables set here to _valid:
    for (auto& v : en->getVariablesSetHere()) {
      _valid.emplace(v);
      _varSetBy->emplace(v->id, en);
    }

    en->setVarsValid(_valid);
    en->setVarUsageValid();
  }

  bool enterSubquery(ExecutionNode*, ExecutionNode* sub) override final {
    VarUsageFinder subfinder(_varSetBy);
    subfinder._valid = _valid;  // need a copy for the subquery!
    sub->walk(&subfinder);

    // we've fully processed the subquery
    return false;
  }
};

/// @brief determine and set _varsUsedLater in all nodes
void ExecutionPlan::findVarUsage() {
  _varSetBy.clear();
  ::VarUsageFinder finder(&_varSetBy);
  root()->walk(&finder);
  // _varSetBy = *finder._varSetBy;

  _varUsageComputed = true;
}

/// @brief determine if the above are already set
bool ExecutionPlan::varUsageComputed() const { return _varUsageComputed; }

/// @brief unlinkNodes, note that this does not delete the removed
/// nodes and that one cannot remove the root node of the plan.
void ExecutionPlan::unlinkNodes(
    std::unordered_set<ExecutionNode*> const& toRemove) {
  for (auto& node : toRemove) {
    unlinkNode(node);
  }
}

/// @brief unlinkNode, note that this does not delete the removed
/// node and that one cannot remove the root node of the plan.
void ExecutionPlan::unlinkNode(ExecutionNode* node, bool allowUnlinkingRoot) {
  auto parents = node->getParents();

  if (parents.empty()) {
    if (!allowUnlinkingRoot) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "Cannot unlink root node of plan");
    }
    // adjust root node. the caller needs to make sure that a new root node gets
    // inserted
    if (node == _root) {
      _root = nullptr;
    }
  }

  auto dep = node->getDependencies();  // Intentionally copy the vector!

  for (auto* p : parents) {
    p->removeDependency(node);

    for (auto* x : dep) {
      TRI_ASSERT(x != nullptr);
      p->addDependency(x);
    }
  }

  for (auto* x : dep) {
    node->removeDependency(x);
  }

  _varUsageComputed = false;
}

/// @brief replaceNode, note that <newNode> must be registered with the plan
/// before this method is called, also this does not delete the old
/// node and that one cannot replace the root node of the plan.
void ExecutionPlan::replaceNode(ExecutionNode* oldNode,
                                ExecutionNode* newNode) {
  TRI_ASSERT(oldNode->id() != newNode->id());
  TRI_ASSERT(newNode->getDependencies().empty());
  TRI_ASSERT(oldNode != _root);

  // Intentional copy
  std::vector<ExecutionNode*> deps = oldNode->getDependencies();

  for (auto* x : deps) {
    TRI_ASSERT(x != nullptr);
    newNode->addDependency(x);
    oldNode->removeDependency(x);
  }

  // Intentional copy
  auto oldNodeParents = oldNode->getParents();

  for (auto* oldNodeParent : oldNodeParents) {
    if (!oldNodeParent->replaceDependency(oldNode, newNode)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "Could not replace dependencies of an old node");
    }
  }
  _varUsageComputed = false;
}

/// @brief insert <newNode> as a new (the first!) dependency of
/// <oldNode> and make the former first dependency of <oldNode> a
/// dependency of <newNode> (and no longer a direct dependency of
/// <oldNode>).
/// <newNode> must be registered with the plan before this method is called.
void ExecutionPlan::insertDependency(ExecutionNode* oldNode,
                                     ExecutionNode* newNode) {
  TRI_ASSERT(oldNode->id() != newNode->id());
  TRI_ASSERT(newNode->getDependencies().empty());
  TRI_ASSERT(oldNode->getDependencies().size() == 1);

  auto oldDeps = oldNode->getDependencies();  // Intentional copy
  TRI_ASSERT(!oldDeps.empty());

  if (!oldNode->replaceDependency(oldDeps[0], newNode)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Could not replace dependencies of an old node");
  }

  newNode->removeDependencies();
  TRI_ASSERT(oldDeps[0] != nullptr);
  newNode->addDependency(oldDeps[0]);
  _varUsageComputed = false;
}

/// @brief create a plan from VPack
ExecutionNode* ExecutionPlan::fromSlice(VPackSlice const& slice) {
  ExecutionNode* ret = nullptr;

  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "plan slice is not an object");
  }

  VPackSlice nodes = slice.get("nodes");

  if (!nodes.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "plan \"nodes\" attribute is not an array");
  }

  // first, re-create all nodes from the Slice, using the node ids
  // no dependency links will be set up in this step
  for (auto const& it : VPackArrayIterator(nodes)) {
    if (!it.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "node entry in plan is not an object");
    }

    ret = ExecutionNode::fromVPackFactory(this, it);
    registerNode(ret);

    TRI_ASSERT(ret != nullptr);

    if (ret->getType() == arangodb::aql::ExecutionNode::SUBQUERY) {
      // found a subquery node. now do magick here
      VPackSlice subquery = it.get("subquery");
      // create the subquery nodes from the "subquery" sub-node
      auto subqueryNode = fromSlice(subquery);

      // register the just created subquery
      static_cast<SubqueryNode*>(ret)->setSubquery(subqueryNode, false);
    }
  }

  // all nodes have been created. now add the dependencies
  for (auto const& it : VPackArrayIterator(nodes)) {
    // read the node's own id
    auto thisId = it.get("id").getNumericValue<size_t>();
    auto thisNode = getNodeById(thisId);

    // now re-link the dependencies
    VPackSlice dependencies = it.get("dependencies");
    if (dependencies.isArray()) {
      for (auto const& it2 : VPackArrayIterator(dependencies)) {
        if (it2.isNumber()) {
          auto depId = it2.getNumericValue<size_t>();
          thisNode->addDependency(getNodeById(depId));
        }
      }
    }
  }

  return ret;
}

/// @brief returns true if a plan is so simple that optimizations would
/// probably cost more than simply executing the plan
bool ExecutionPlan::isDeadSimple() const {
  auto current = _root;

  while (current != nullptr) {
    auto const nodeType = current->getType();

    if (nodeType == ExecutionNode::SUBQUERY ||
        nodeType == ExecutionNode::ENUMERATE_COLLECTION ||
        nodeType == ExecutionNode::ENUMERATE_LIST ||
        nodeType == ExecutionNode::TRAVERSAL ||
        nodeType == ExecutionNode::SHORTEST_PATH ||
        nodeType == ExecutionNode::INDEX) {
      // these node types are not simple
      return false;
    }

    auto dep = current->getFirstDependency();

    if (dep == nullptr) {
      break;
    }

    current = dep;
  }

  return true;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#include <iostream>

/// @brief show an overview over the plan
struct Shower final : public WalkerWorker<ExecutionNode> {
  int indent;

  Shower() : indent(0) {}

  ~Shower() {}

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    indent++;
    return true;
  }

  void leaveSubquery(ExecutionNode*, ExecutionNode*) override final {
    indent--;
  }

  void after(ExecutionNode* en) override final {
    for (int i = 0; i < indent; i++) {
      std::cout << ' ';
    }
    std::cout << en->getTypeString() << std::endl;
  }
};

/// @brief show an overview over the plan
void ExecutionPlan::show() {
  Shower shower;
  _root->walk(&shower);
}

#endif
