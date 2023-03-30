////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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

#include "GraphOptimizerRules.h"

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/QueryContext.h"
#include "Aql/Optimizer.h"
#include "Aql/TraversalNode.h"
#include "Containers/SmallVector.h"


#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
bool isSupportedNode(Ast const* ast, Variable const* pathVar,
                     AstNode const* node) {
  // do a quick first check for all comparisons
  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
    case NODE_TYPE_OPERATOR_BINARY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_NE:
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
    case NODE_TYPE_OPERATOR_BINARY_IN:
    case NODE_TYPE_OPERATOR_BINARY_NIN: {
      // the following types of expressions are not supported
      //   p.edges[0]._from  op  whatever attribute access
      //   whatever attribute access  op  p.edges[0]._from
      AstNode const* lhs = node->getMember(0);
      AstNode const* rhs = node->getMember(1);

      if (lhs->isAttributeAccessForVariable(pathVar, true)) {
        // p.xxx  op  whatever
        if (rhs->type != NODE_TYPE_VALUE && rhs->type != NODE_TYPE_ARRAY &&
            rhs->type != NODE_TYPE_OBJECT && rhs->type != NODE_TYPE_REFERENCE) {
          return false;
        }
      } else if (rhs->isAttributeAccessForVariable(pathVar, true)) {
        // whatever  op  p.xxx
        if (lhs->type != NODE_TYPE_VALUE && lhs->type != NODE_TYPE_ARRAY &&
            lhs->type != NODE_TYPE_OBJECT && lhs->type != NODE_TYPE_REFERENCE) {
          return false;
        }
      }
      break;
    }
    default: {
      // intentionally no other cases defined...
      // we'll simply fall through to the next switch..case statement
      break;
    }
  }

  switch (node->type) {
    case NODE_TYPE_VARIABLE:
    case NODE_TYPE_OPERATOR_UNARY_PLUS:
    case NODE_TYPE_OPERATOR_UNARY_MINUS:
    case NODE_TYPE_OPERATOR_UNARY_NOT:
    case NODE_TYPE_OPERATOR_BINARY_AND:
    case NODE_TYPE_OPERATOR_BINARY_OR:
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
    case NODE_TYPE_OPERATOR_BINARY_DIV:
    case NODE_TYPE_OPERATOR_BINARY_MOD:
    case NODE_TYPE_OPERATOR_BINARY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_NE:
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
    case NODE_TYPE_OPERATOR_BINARY_IN:
    case NODE_TYPE_OPERATOR_BINARY_NIN:
    case NODE_TYPE_ATTRIBUTE_ACCESS:
    case NODE_TYPE_BOUND_ATTRIBUTE_ACCESS:
    case NODE_TYPE_INDEXED_ACCESS:
    case NODE_TYPE_EXPANSION:
    case NODE_TYPE_ITERATOR:
    case NODE_TYPE_VALUE:
    case NODE_TYPE_ARRAY:
    case NODE_TYPE_OBJECT:
    case NODE_TYPE_OBJECT_ELEMENT:
    case NODE_TYPE_REFERENCE:
    case NODE_TYPE_NOP:
    case NODE_TYPE_RANGE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
    case NODE_TYPE_QUANTIFIER:
    case NODE_TYPE_ARRAY_FILTER:
      return true;
    case NODE_TYPE_FCALL: {
      auto* func = static_cast<Function const*>(node->getData());
      if (!func->hasFlag(Function::Flags::Deterministic)) {
        // non-deterministic functions will never be pulled into the
        // traversal
        return false;
      }
      if (!ServerState::instance()->isRunningInCluster()) {
        return true;
      }
      // only allow those functions that can be executed on DB servers
      // as well
      if (ast->query().vocbase().isOneShard()) {
        return func->hasFlag(Function::Flags::CanRunOnDBServerOneShard);
      }
      return func->hasFlag(Function::Flags::CanRunOnDBServerCluster);
    }
    case NODE_TYPE_FCALL_USER:
      // JavaScript user-defined functions will never be pulled into the
      // traversal
      return false;
    case NODE_TYPE_OPERATOR_NARY_OR:
    case NODE_TYPE_OPERATOR_NARY_AND:
      // If we get here the astNode->normalize() did not work
      TRI_ASSERT(false);
      return false;
    default:
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      LOG_TOPIC("ebe25", ERR, arangodb::Logger::FIXME)
          << "Traversal optimizer encountered node: " << node->getTypeString();
#endif
      return false;
  }
}

enum class PathAccessState {
  // Search for access to any path variable
  SEARCH_ACCESS_PATH,
  // Check if we access edges or vertices on it
  ACCESS_EDGES_OR_VERTICES,
  // Check if we have an indexed access [x] (we find the x here)
  SPECIFIC_DEPTH_ACCESS_DEPTH,
  // CHeck if we have an indexed access [x] (we find [ ] here)
  SPECIFIC_DEPTH_ACCESS_INDEXED_ACCESS,
};

auto swapOutLastElementAccesses(
    Ast* ast,
    AstNode* condition,
    std::unordered_map<Variable const*,
                       std::pair<Variable const*, Variable const*>>
        pathVariables) -> std::pair<bool, AstNode*> {
  Variable const* matchingPath = nullptr;
  bool isEdgeAccess = false;
  bool appliedAChange = false;

  PathAccessState currentState{PathAccessState::SEARCH_ACCESS_PATH};

  auto searchAccessPattern = [&](AstNode* node) -> AstNode* {
    switch (currentState) {
      case PathAccessState::SEARCH_ACCESS_PATH: {
        if (node->type == NODE_TYPE_REFERENCE ||
            node->type == NODE_TYPE_VARIABLE) {
          // we are on the bottom of the tree. Check if it is our pathVar
          auto variable = static_cast<Variable*>(node->getData());
          if (pathVariables.contains(variable)) {
            currentState = PathAccessState::ACCESS_EDGES_OR_VERTICES;
            matchingPath = variable;
          }
        }
        // Keep for now
        return node;
      }
      case PathAccessState::ACCESS_EDGES_OR_VERTICES: {
        // we have var.<this-here>
        // Only vertices || edges supported
        if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          if (node->stringEquals(StaticStrings::GraphQueryEdges)) {
            isEdgeAccess = true;
            currentState = PathAccessState::SPECIFIC_DEPTH_ACCESS_DEPTH;
            return node;
          } else if (node->stringEquals(StaticStrings::GraphQueryVertices)) {
            isEdgeAccess = false;
            currentState = PathAccessState::SPECIFIC_DEPTH_ACCESS_DEPTH;
            return node;
          }
        }
        // Incorrect type, do not touch, abort this search,
        // setup next one
        currentState = PathAccessState::SEARCH_ACCESS_PATH;
        return node;
      }
      case PathAccessState::SPECIFIC_DEPTH_ACCESS_DEPTH: {
        // we have var.edges[<this-here>], we can only let -1 pass here for
        // optimization
        if (node->value.type == VALUE_TYPE_INT &&
            node->value.value._int == -1) {
          currentState = PathAccessState::SPECIFIC_DEPTH_ACCESS_INDEXED_ACCESS;
          return node;
        }
        // Incorrect type, do not touch, abort this search,
        // setup next one
        currentState = PathAccessState::SEARCH_ACCESS_PATH;
        return node;
      }
      case PathAccessState::SPECIFIC_DEPTH_ACCESS_INDEXED_ACCESS: {
        // We are in xxxx[-1] pattern, the next node has to be the indexed
        // access.
        if (node->type == NODE_TYPE_INDEXED_ACCESS) {
          // Reset pattern, we can now restart the search
          currentState = PathAccessState::SEARCH_ACCESS_PATH;

          // Let's switch!!
          appliedAChange = true;
          // We searched for the very same variable above!
          auto const& matchingElements = pathVariables.find(matchingPath);
          return ast->createNodeReference(isEdgeAccess
                                              ? matchingElements->second.second
                                              : matchingElements->second.first);
        }
        currentState = PathAccessState::SEARCH_ACCESS_PATH;
        return node;
      }
    }
  };

  auto newCondition = Ast::traverseAndModify(condition, searchAccessPattern);
  if (newCondition != condition) {
    // Swap out everything.
    return std::make_pair(appliedAChange, newCondition);
  }

  return std::make_pair(appliedAChange, nullptr);
}

}

auto PathVariableAccess::isLast() const noexcept -> bool {
  return std::visit(overload{[](AllAccess const&) { return false; },
                             [](int64_t const& i) { return i == -1; }},
                    index);
}

auto PathVariableAccess::getDepth() const noexcept -> int64_t {
  ADB_PROD_ASSERT(std::holds_alternative<int64_t>(index));
  return std::get<int64_t>(index);
}

auto PathVariableAccess::isAllAccess() const noexcept -> bool {
  return std::holds_alternative<AllAccess>(index);
}

auto PathVariableAccess::isEdgeAccess() const noexcept -> bool {
  return type == AccessType::EDGE;
}

auto PathVariableAccess::isVertexAccess() const noexcept -> bool {
  return type == AccessType::EDGE;
}

auto arangodb::aql::maybeExtractPathAccess(Ast* ast, Variable const* pathVar, AstNode* parent, size_t testIndex)
    -> std::optional<PathVariableAccess> {
  AstNode* node = parent->getMemberUnchecked(testIndex);
  if (!isSupportedNode(ast, pathVar, node)) {
    return std::nullopt;
  }

  int64_t indexedAccessDepth = 0;

  // We need to walk through each branch and validate:
  // 1. It does not contain unsupported types
  // 2. Only one contains var
  // 3. The one with var matches pattern:
  //   A) var.vertices[n] (.*)
  //   B) var.edges[n] (.*)
  //   C) var.vertices[*] (.*) (ALL|NONE) (.*)
  //   D) var.edges[*] (.*) (ALL|NONE) (.*)

  auto unusedWalker = [](AstNode const* n) {};
  bool isEdge = false;
  // We define that depth == UINT64_MAX is "ALL depths"
  uint64_t depth = UINT64_MAX;
  AstNode* parentOfReplace = nullptr;
  size_t replaceIdx = 0;
  bool notSupported = false;

  // We define that patternStep >= 6 is complete Match.
  unsigned char patternStep = 0;

  auto supportedGuard = [&ast, &notSupported,
                         pathVar](AstNode const* n) -> bool {
    // cppcheck-suppress knownConditionTrueFalse
    if (notSupported) {
      return false;
    }
    if (!isSupportedNode(ast, pathVar, n)) {
      notSupported = true;
      return false;
    }
    return true;
  };

  auto searchPattern = [&patternStep, &isEdge, &depth, &pathVar, &notSupported,
                        &parentOfReplace, &replaceIdx,
                        &indexedAccessDepth](AstNode* node) -> AstNode* {
    if (notSupported) {
      // Short circuit, this condition cannot be fulfilled.
      return node;
    }
    switch (patternStep) {
      case 1:
        // we have var.<this-here>
        // Only vertices || edges supported
        if (node->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
          // Incorrect type
          notSupported = true;
          return node;
        }
        if (node->stringEquals(StaticStrings::GraphQueryEdges)) {
          isEdge = true;
        } else if (node->stringEquals(StaticStrings::GraphQueryVertices)) {
          isEdge = false;
        } else {
          notSupported = true;
          return node;
        }
        patternStep++;
        return node;
      case 2: {
        switch (node->type) {
          case NODE_TYPE_VALUE: {
            // we have var.edges[<this-here>]
            if (node->value.type != VALUE_TYPE_INT ||
                node->value.value._int < 0) {
              // Only positive indexed access allowed
              notSupported = true;
              return node;
            }
            depth = static_cast<uint64_t>(node->value.value._int);
            break;
          }
          case NODE_TYPE_ITERATOR:
          case NODE_TYPE_REFERENCE:
            // This Node type is ok. it does not convey any information
            break;
          default:
            // Other types cannot be optimized
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
            LOG_TOPIC("fcdf3", ERR, arangodb::Logger::FIXME)
                << "Failed type: " << node->getTypeString();
            node->dump(0);
#endif
            notSupported = true;
            return node;
        }
        patternStep++;
        break;
      }
      case 3:
        if (depth != UINT64_MAX) {
          // We are in depth pattern.
          // The first Node we encount HAS to be indexed Access
          if (node->type != NODE_TYPE_INDEXED_ACCESS) {
            notSupported = true;
            return node;
          }
          // This completes this pattern. All good
          // Search for the parent having this node.
          patternStep = 6;
          parentOfReplace = node;

          // we need to know the depth at which a filter condition will
          // access a path. Otherwise there are too many results
          TRI_ASSERT(node->numMembers() == 2);
          AstNode* indexVal = node->getMemberUnchecked(1);
          if (indexVal->isIntValue()) {
            indexedAccessDepth = indexVal->getIntValue() + (isEdge ? 1 : 0);
          } else {  // should cause the caller to not remove a filter
            indexedAccessDepth = INT64_MAX;
          }
          return node;
        }
        if (node->type == NODE_TYPE_EXPANSION) {
          // Check that the expansion [*] contains no inline expression;
          // members 2, 3 and 4 correspond to FILTER, LIMIT and RETURN,
          // respectively.
          TRI_ASSERT(node->numMembers() == 5);
          if (node->getMemberUnchecked(2)->type != NODE_TYPE_NOP ||
              node->getMemberUnchecked(3)->type != NODE_TYPE_NOP ||
              node->getMemberUnchecked(4)->type != NODE_TYPE_NOP) {
            notSupported = true;
            return node;
          }

          // We continue in this pattern, all good
          patternStep++;
          parentOfReplace = node;
          return node;
        }
        // if we get here we are in the expansion operator.
        // We simply pipe this one through
        break;
      case 4: {
        if (node->type == NODE_TYPE_QUANTIFIER) {
          // We are in array case. We need to wait for a quantifier
          // This means we have path.edges[*] on the right hand side
          if (Quantifier::isAny(node)) {
            // Nono optimize for ANY
            notSupported = true;
            return node;
          }
          // This completes this pattern. All good
          // Search for the parent having this node.
          patternStep = 5;
        }
        // if we get here we are in the expansion operator.
        // We simply pipe this one through
        break;
      }
      case 5:
      case 6: {
        for (size_t idx = 0; idx < node->numMembers(); ++idx) {
          if (node->getMemberUnchecked(idx) == parentOfReplace) {
            if (patternStep == 5) {
              if (idx != 0) {
                // We found a right hand side expansion of y ALL == p.edges[*]
                // We cannot optimize this
                notSupported = true;
                return node;
              }
            }
            parentOfReplace = node;
            replaceIdx = idx;
            // Ok finally done.
            patternStep++;
            break;
          }
        }
      }
      default:
        // Just fall through
        break;
    }
    if (node->type == NODE_TYPE_REFERENCE || node->type == NODE_TYPE_VARIABLE) {
      // we are on the bottom of the tree. Check if it is our pathVar
      auto variable = static_cast<Variable*>(node->getData());
      if (pathVar == variable) {
        // We found pathVar
        if (patternStep != 0) {
          // found it twice. Abort
          notSupported = true;
          return node;
        }
        ++patternStep;
      }
    }
    return node;
  };

  // Check branches:
  size_t numMembers = node->numMembers();
  for (size_t i = 0; i < numMembers; ++i) {
    Ast::traverseAndModify(node->getMemberUnchecked(i), supportedGuard,
                           searchPattern, unusedWalker);
    if (notSupported) {
      return std::nullopt;
    }
    if (patternStep == 5) {
      // The first item is direct child of the parent.
      // Use parent to replace
      // This is only the case on Expansion being
      // the node we have to replace.
      TRI_ASSERT(parentOfReplace->type == NODE_TYPE_EXPANSION);
      if (parentOfReplace != node->getMemberUnchecked(0)) {
        // We found a right hand side of x ALL == p.edges[*]
        // Cannot optimize
        return std::nullopt;
      }
      parentOfReplace = node;
      replaceIdx = 0;
      patternStep++;
    }
    if (patternStep == 6) {
      if (parentOfReplace == node) {
        parentOfReplace = parent;
        replaceIdx = testIndex;
      } else {
        TRI_ASSERT(parentOfReplace == node->getMemberUnchecked(i));
        parentOfReplace = node;
        replaceIdx = i;
      }
      patternStep++;
    }
    if (patternStep == 7) {
      patternStep++;
    }
  }

  if (patternStep < 8) {
    // We found sth. that is not matching the pattern complete.
    // => Do not optimize
    return std::nullopt;
  }

  // If we get here we found a proper PathAccess, let's build it
  PathVariableAccess pathAccess{};
  if (isEdge) {
    pathAccess.type = PathVariableAccess::AccessType::EDGE;
  } else {
    pathAccess.type = PathVariableAccess::AccessType::VERTEX;
  }
  if (depth == UINT64_MAX) {
    pathAccess.index = PathVariableAccess::AllAccess{};
  } else {
    // TODO: Fix ME:
    pathAccess.index = static_cast<int64_t>(depth);
  }
  pathAccess.parentOfReplace = parentOfReplace;
  pathAccess.replaceIdx = replaceIdx;
  return pathAccess;
}

void arangodb::aql::replaceLastAccessOnGraphPathRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  // Only pick Traversal nodes, all others do not provide allowed output format
  containers::SmallVector<ExecutionNode*, 8> tNodes;
  plan->findNodesOfType(tNodes, ExecutionNode::TRAVERSAL, true);

  if (tNodes.empty()) {
    // no traversals present
    opt->addPlan(std::move(plan), rule, false);
    return;
  }
  // Unfortunately we do not have a reverse lookup on where a variable is used.
  // So we first select all traversal candidates, and afterwards check where
  // they are used.
  std::unordered_map<Variable const*, std::pair<Variable const*, Variable const*>> candidates;
  candidates.reserve(tNodes.size());
  for (auto const& n : tNodes) {
    auto* traversal = ExecutionNode::castTo<TraversalNode*>(n);

    if (traversal->isPathOutVariableUsedLater()) {
      auto pathOutVariable = traversal->pathOutVariable();
      TRI_ASSERT(pathOutVariable != nullptr);
      // Without further optimization an accessible path variable, requires vertex and edge to be accessible.
      TRI_ASSERT(traversal->vertexOutVariable() != nullptr);
      TRI_ASSERT(traversal->edgeOutVariable() != nullptr);
      candidates.emplace(pathOutVariable, std::make_pair(traversal->vertexOutVariable(), traversal->edgeOutVariable()));
      LOG_DEVEL << "Identified Candidate " << traversal->id();
    }
  }

  if (candidates.empty()) {
    // no traversals with path output present nothing to be done
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  // Path Access always has to be in a Calculation Node.
  // So let us check for those
  tNodes.clear();
  plan->findNodesOfType(tNodes, ExecutionNode::CALCULATION, true);

  if (tNodes.empty()) {
    // no calculations, so no one that could access the path
    opt->addPlan(std::move(plan), rule, false);
    return;
  }

  bool appliedAChange = false;
  for (auto& n : tNodes) {
    auto* calculation = ExecutionNode::castTo<CalculationNode*>(n);
    auto [didApplyChange, replacementCondition] = swapOutLastElementAccesses(plan->getAst(), calculation->expression()->nodeForModification(), candidates);
    // Get's true as soon as one of the swapOut calls returns true
    appliedAChange |= didApplyChange;
    if (replacementCondition != nullptr) {
      // This is the indicator that we have to replace the full expression here.
      calculation->expression()->replaceNode(replacementCondition);
    }
  }
  // Nothing done
  opt->addPlan(std::move(plan), rule, appliedAChange);
}
