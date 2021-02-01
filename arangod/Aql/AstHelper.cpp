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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "AstHelper.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

auto doNothingVisitor = [](AstNode const*) {};

bool accessesSearchVariableViaReference(AstNode const* current, Variable const* searchVariable) {
  if (current->type == NODE_TYPE_INDEXED_ACCESS) {
    auto sub = current->getMemberUnchecked(0);
    if (sub->type == NODE_TYPE_REFERENCE) {
      Variable const* v = static_cast<Variable const*>(sub->getData());
      if (v->id == searchVariable->id) {
        return true;
      }
    }
  } else if (current->type == NODE_TYPE_EXPANSION) {
    if (current->numMembers() < 2) {
      return false;
    }
    auto it = current->getMemberUnchecked(0);
    if (it->type != NODE_TYPE_ITERATOR || it->numMembers() != 2) {
      return false;
    }

    auto sub1 = it->getMember(0);
    auto sub2 = it->getMember(1);
    if (sub1->type != NODE_TYPE_VARIABLE || sub2->type != NODE_TYPE_REFERENCE) {
      return false;
    }
    Variable const* v = static_cast<Variable const*>(sub2->getData());
    if (v->id == searchVariable->id) {
      return true;
    }
  }

  return false;
};

bool isTargetVariable(AstNode const* node,
                      ::arangodb::containers::SmallVector<Variable const*>& searchVariables,
                      bool& isSafeForOptimization) {
  TRI_ASSERT(!searchVariables.empty());

  // given and expression like g3[0].`g2`[0].`g1`[0].`item1`.`_id`
  // this loop resolves subtrees of the form: .`g2`[0].`g1`[0]
  // search variables should equal [g1, g2, g3]. Therefor we need not match the
  // variable names from the vector with those in the expression while going
  // forward in the vector the we go backward in the expression

  auto current = node;
  for (auto varIt = searchVariables.begin();
       varIt != std::prev(searchVariables.end()); ++varIt) {
    AstNode* next = nullptr;
    if (current->type == NODE_TYPE_INDEXED_ACCESS) {
      next = current->getMemberUnchecked(0);
    } else if (current->type == NODE_TYPE_EXPANSION) {
      if (current->numMembers() < 2) {
        return false;
      }

      auto it = current->getMemberUnchecked(0);
      TRI_ASSERT(it);

      // The expansion is at the very end
      if (it->type == NODE_TYPE_ITERATOR && it->numMembers() == 2) {
        if (it->getMember(0)->type != NODE_TYPE_VARIABLE) {
          return false;
        }

        auto attributeAccess = it->getMember(1);
        TRI_ASSERT(attributeAccess);

        if (std::next(varIt) != searchVariables.end() &&
            attributeAccess->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          next = attributeAccess;
        } else {
          return false;
        }

      } else {
        // The expansion is not at the very end! we are unable to check if the
        // variable will be accessed checking nested expansions is really crazy
        // and would probably be best done in a recursive way. If the expansion
        // is in the middle, then it would be still the very first node in the
        // AST, having 2 subtrees that contain the other search variables
        isSafeForOptimization = false;  // could be an access - we can not tell
        return false;
      }
    } else {
      return false;
    }

    if (varIt == searchVariables.end()) {
      return false;
    }

    if (next && next->type == NODE_TYPE_ATTRIBUTE_ACCESS &&
        next->getString() == (*varIt)->name) {
      current = next->getMemberUnchecked(0);
    } else if (next && next->type == NODE_TYPE_EXPANSION) {
      isSafeForOptimization = false;
      return false;
    } else {
      return false;
    }
  }  // for nodes but last

  if (!current) {
    return false;
  }

  return accessesSearchVariableViaReference(current, searchVariables.back());
}

}  // namespace

std::unordered_set<std::string> arangodb::aql::ast::getReferencedAttributesForKeep(
    const AstNode* node, ::arangodb::containers::SmallVector<const Variable*, 64> searchVariables,
    bool& isSafeForOptimization) {
  std::unordered_set<std::string> result;
  isSafeForOptimization = true;

  std::function<bool(AstNode const*)> visitor = [&isSafeForOptimization, &result,
                                                 &searchVariables](AstNode const* node) {
    if (!isSafeForOptimization) {
      return false;
    }

    if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      while (node->getMemberUnchecked(0)->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        node = node->getMemberUnchecked(0);
      }
      if (isTargetVariable(node->getMemberUnchecked(0), searchVariables, isSafeForOptimization)) {
        result.emplace(node->getString());
        // do not descend further
        return false;
      }
    } else if (node->type == NODE_TYPE_REFERENCE) {
      Variable const* v = static_cast<Variable const*>(node->getData());
      if (v->id == searchVariables.front()->id) {
        isSafeForOptimization = false;  // the expression references the searched variable
        return false;
      }
    } else if (node->type == NODE_TYPE_EXPANSION) {
      if (isTargetVariable(node, searchVariables, isSafeForOptimization)) {
        auto sub = node->getMemberUnchecked(1);
        if (sub->type == NODE_TYPE_EXPANSION) {
          sub = sub->getMemberUnchecked(0)->getMemberUnchecked(1);
        }
        if (sub->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          while (sub->getMemberUnchecked(0)->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
            sub = sub->getMemberUnchecked(0);
          }
          result.emplace(sub->getString());
          // do not descend further
          return false;
        }
      }
    } else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
      auto sub = node->getMemberUnchecked(0);
      if (sub->type == NODE_TYPE_REFERENCE) {
        Variable const* v = static_cast<Variable const*>(sub->getData());
        if (v->id == searchVariables.back()->id) {
          isSafeForOptimization = false;
          return false;
        }
      }
    }
    return true;
  };

  // Traverse AST and call visitor before recursing on each node
  // as long as visitor returns true the traversal continues. In
  // that branch
  Ast::traverseReadOnly(node, visitor, ::doNothingVisitor);

  return result;
}
