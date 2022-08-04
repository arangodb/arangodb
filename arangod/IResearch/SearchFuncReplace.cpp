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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "SearchFuncReplace.h"

#include "Aql/Ast.h"
#include "Aql/IResearchViewNode.h"

namespace arangodb::iresearch {

void extractSearchFunc(IResearchViewNode const& viewNode,
                       DedupSearchFuncs& dedup,
                       std::vector<SearchFunc>& funcs) {
  auto* viewVar = &viewNode.outVariable();
  arangodb::aql::VarSet usedVars;

  for (auto it = std::begin(dedup); it != std::end(dedup);) {
    auto& func = it->first;
    if (func.var == viewVar) {
      // extract all variables used in scorer
      usedVars.clear();
      aql::Ast::getReferencedVariables(func.node, usedVars);

      // get all variables valid in view node
      auto const& validVars = viewNode.getVarsValid();
      for (auto* v : usedVars) {
        if (!validVars.contains(v)) {
          TRI_ASSERT(func.node);
          auto const funcName = iresearch::getFuncName(*func.node);

          THROW_ARANGO_EXCEPTION_FORMAT(
              TRI_ERROR_BAD_PARAMETER,
              "Inaccesible non-ArangoSearch view variable '%s' is used in "
              "search function '%s'",
              v->name.c_str(), funcName.c_str());
        }
      }

      funcs.emplace_back(it->second, func.node);
      const auto copy_it = it++;
      dedup.erase(copy_it);
    } else {
      ++it;
    }
  }
}

aql::Variable const* getSearchFuncRef(aql::AstNode const* args) noexcept {
  if (!args || aql::NODE_TYPE_ARRAY != args->type) {
    return nullptr;
  }

  size_t const size = args->numMembers();

  if (size < 1) {
    return nullptr;  // invalid args
  }

  // 1st argument has to be reference to `ref`
  auto const* arg0 = args->getMemberUnchecked(0);

  if (!arg0 || aql::NODE_TYPE_REFERENCE != arg0->type) {
    return nullptr;
  }

  for (size_t i = 1, size = args->numMembers(); i < size; ++i) {
    auto const* arg = args->getMemberUnchecked(i);

    if (!arg || !arg->isDeterministic()) {
      // we don't support non-deterministic arguments for scorers
      return nullptr;
    }
  }

  return reinterpret_cast<aql::Variable const*>(arg0->getData());
}

}  // namespace arangodb::iresearch
