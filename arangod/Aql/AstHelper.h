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
#include "Logger/Logger.h"

#include <unordered_set>

namespace {
   auto doNothingVisitor = [](arangodb::aql::AstNode const*) {};
}


namespace arangodb {
namespace aql {
namespace ast {

namespace {

LoggerStream& operator<<(LoggerStream& os, SmallVector<Variable const*> const vec) {
  os << "[ ";
  for(auto const& var: vec) {
    os << var->name << " ";
  }
  return os << "]";
}

bool checkVaildVaribaleAccess(AstNode const* current, Variable const* searchVariable) {

	// now we need to check if the last variable is
	// referende in the expression

  if (current->type == NODE_TYPE_INDEXED_ACCESS) {
    auto sub = current->getMemberUnchecked(0);
    if (sub->type == NODE_TYPE_REFERENCE) {
      Variable const* v = static_cast<Variable const*>(sub->getData());
      if (v->id == searchVariable->id) {
				LOG_DEVEL << "is target true";
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
			LOG_DEVEL << "is target true";
      return true;
    }
  }

  return false;
};

bool isTargetVariable(AstNode const* node, SmallVector<Variable const*>& searchVariables, bool& isSafeForOptimization) {
	TRI_ASSERT(!searchVariables.empty());

	auto current = node;
	for(auto varIt = searchVariables.begin(); varIt != std::prev(searchVariables.end()); ++varIt) {
		AstNode* next = nullptr;
		if (current->type == NODE_TYPE_INDEXED_ACCESS) {
      next = current->getMemberUnchecked(0);
		} else if (current->type == NODE_TYPE_EXPANSION) {
		  current->dump(8);

      if (current->numMembers() < 2) {
        return false;
      }

      auto it = current->getMemberUnchecked(0);
      TRI_ASSERT(it);

      //The expansion is at the very end
      if (it->type == NODE_TYPE_ITERATOR && it->numMembers() == 2) {
        LOG_DEVEL << "got iterator";

        if (it->getMember(0)->type != NODE_TYPE_VARIABLE ) {
				  return false;
			  }

        auto attributeAccess = it->getMember(1);
        TRI_ASSERT(attributeAccess);

        if (std::next(varIt) != searchVariables.end() && attributeAccess->type == NODE_TYPE_ATTRIBUTE_ACCESS){
            next = attributeAccess;
        } else {
          return false;
        }

      } else {
        // the expansion is not at the very end
        // we are unable to check if the variable will be accessed
        isSafeForOptimization = false;
        return false;
      }
		} else {
			return false;
		}

    if(varIt == searchVariables.end()) {
      return false;
    }

		if (next && next->type == NODE_TYPE_ATTRIBUTE_ACCESS &&
			  next->getString() == (*varIt)->name ) {
				current = next->getMemberUnchecked(0);
		} else if (next && next->type == NODE_TYPE_EXPANSION) {
        isSafeForOptimization = false;
        return false;
		} else {
      return false;
    }
	} // for nodes but last

  if(!current) {
    return false;
  }

  return checkVaildVaribaleAccess(current, searchVariables.back());
}

} // end - namespace unnames


/// @brief determines the to-be-kept attribute of an INTO expression
inline std::unordered_set<std::string> getReferencedAttributesForKeep(
      AstNode const* node, SmallVector<Variable const*> searchVariables, bool& isSafeForOptimization){

  std::unordered_set<std::string> result;
  isSafeForOptimization = true;

  std::function<bool(AstNode const*)> visitor = [&isSafeForOptimization,
                                                 &result,
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
        isSafeForOptimization = false; // the expression references the searched variable
        return false;
      }
    } else if (node->type == NODE_TYPE_EXPANSION) {
      if (isTargetVariable(node, searchVariables, isSafeForOptimization)) {
        LOG_DEVEL << "Is expand target";
        auto sub = node->getMemberUnchecked(1);
        if (sub->type == NODE_TYPE_EXPANSION) {
          sub = sub->getMemberUnchecked(0)->getMemberUnchecked(1);
        }
        if (sub->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          while (sub->getMemberUnchecked(0)->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
            sub = sub->getMemberUnchecked(0);
          }
          result.emplace(sub->getString());
          LOG_DEVEL << "adding " << sub->getString();
          // do not descend further
          return false;
        }
      } else {
        LOG_DEVEL << "is not target variable in expansion";
      }
    }

    return true;
  };

  // traverse ast and call visitor before recursing on each node
  // as long as visitor returns true the traversal continues
  Ast::traverseReadOnly(node, visitor, ::doNothingVisitor);

  return result;
}


} // end - namsepace ast
} // end - namespace aql
} // end - namespace arangodb

#endif
