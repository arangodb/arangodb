////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

bool isTargetVariable(AstNode const* const current,
                      Variable const* const searchVariable) {
  if (current->type == NODE_TYPE_INDEXED_ACCESS) {
    auto sub = current->getMemberUnchecked(0);
    if (sub->type == NODE_TYPE_REFERENCE) {
      auto const v = static_cast<Variable const*>(sub->getData());
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
    auto const v = static_cast<Variable const*>(sub2->getData());
    if (v->id == searchVariable->id) {
      return true;
    }
  }

  return false;
};

}  // namespace

auto arangodb::aql::ast::getReferencedAttributesForKeep(
    AstNode const* const node, Variable const* const searchVariable,
    bool& isSafeForOptimization) -> std::vector<std::string> {
  auto result = std::vector<std::string>();
  isSafeForOptimization = true;

  auto const visitor = [&isSafeForOptimization, &result,
                        &searchVariable](AstNode const* node) -> bool {
    if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      while (node->getMemberUnchecked(0)->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        node = node->getMemberUnchecked(0);
      }
      if (isTargetVariable(node->getMemberUnchecked(0), searchVariable)) {
        result.emplace_back(node->getString());
        // do not descend further
        return false;
      }
    } else if (node->type == NODE_TYPE_REFERENCE) {
      auto const v = static_cast<Variable const*>(node->getData());
      if (v->id == searchVariable->id) {
        isSafeForOptimization =
            false;  // the expression references the searched variable
        return false;
      }
    } else if (node->type == NODE_TYPE_EXPANSION) {
      if (isTargetVariable(node, searchVariable)) {
        auto sub = node->getMemberUnchecked(1);
        if (sub->type == NODE_TYPE_EXPANSION) {
          sub = sub->getMemberUnchecked(0)->getMemberUnchecked(1);
        }
        if (sub->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          while (sub->getMemberUnchecked(0)->type ==
                 NODE_TYPE_ATTRIBUTE_ACCESS) {
            sub = sub->getMemberUnchecked(0);
          }
          result.emplace_back(sub->getString());
          // do not descend further
          return false;
        }
      }
    } else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
      auto sub = node->getMemberUnchecked(0);
      if (sub->type == NODE_TYPE_REFERENCE) {
        auto const v = static_cast<Variable const*>(sub->getData());
        if (v->id == searchVariable->id) {
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
