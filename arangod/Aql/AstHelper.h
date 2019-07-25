////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_AST_HELPER_H
#define ARANGOD_AQL_AST_HELPER_H 1

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "Basics/SmallVector.h"

#include <unordered_set>

namespace {
   auto doNothingVisitor = [](arangodb::aql::AstNode const*) {};
}


namespace arangodb {
namespace aql {
namespace ast {

/// @brief determines the to-be-kept attribute of an INTO expression
//
// - adds attribute accesses to `searchVariable` (e.g. searchVar.attribute) in expression given by `node` to return value `results`
// - if a node references the search variable in the expression `isSafeForOptimization` is set to false
//   and the traversal stops.
// - adds expansion // TODO
inline std::unordered_set<std::string> getReferencedAttributesForKeep(
      AstNode const* node, Variable const* searchVariable, bool& isSafeForOptimization, bool checkForAttributeName = false){

  // Inspects ast nodes if the node in question is not of type
  // NODE_TYPE_INDEXED_ACCESS or NODE_TYPE_EXPANSION the lambda return
  // immediately false. Otherwise members are checked they contain a variable
  // or a reference to one. If true the found variable is checked against the
  // search variable. The result of the comparison is returned.
  auto isTargetVariable = [&searchVariable](AstNode const* node) {
    if (node->type == NODE_TYPE_INDEXED_ACCESS) {
      auto sub = node->getMemberUnchecked(0);
      if (sub->type == NODE_TYPE_REFERENCE) {
        Variable const* v = static_cast<Variable const*>(sub->getData());
        if (v->id == searchVariable->id) {
          return true;
        }
      }
    } else if (node->type == NODE_TYPE_EXPANSION) {
      if (node->numMembers() < 2) {
        return false;
      }
      auto it = node->getMemberUnchecked(0);
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

  std::unordered_set<std::string> result;
  isSafeForOptimization = true;

  std::function<bool(AstNode const*)> visitor = [&isSafeForOptimization,
                                                 &result, &isTargetVariable,
                                                 &searchVariable, checkForAttributeName](AstNode const* node) {
    ///LOG_DEVEL << "@@@ visiting " << node;
    if (!isSafeForOptimization) {
      return false;
    }

    if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      while (node->getMemberUnchecked(0)->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        node = node->getMemberUnchecked(0);
      }
      if (isTargetVariable(node->getMemberUnchecked(0))) {
        result.emplace(node->getString());
        // do not descend further
        return false;
      }
    } else if (node->type == NODE_TYPE_REFERENCE) {
      Variable const* v = static_cast<Variable const*>(node->getData());
      if (v->id == searchVariable->id) {
        isSafeForOptimization = false; // the expression references the searched variable
        return false;
      }
    } else if (node->type == NODE_TYPE_EXPANSION) {
      if (isTargetVariable(node)) {
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
    }

    return true;
  };

  // traverse ast and call visitor before recursing on each node
  // as long as visitor returns true the traversal continues
  Ast::traverseReadOnly(node, visitor, ::doNothingVisitor);

  return result;
}


}
}
}

#endif
