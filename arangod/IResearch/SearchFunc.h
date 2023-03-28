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

#pragma once

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Expression.h"
#include "Containers/FlatHashMap.h"
#include "IResearch/AqlHelper.h"

namespace arangodb::iresearch {

// Represents IResearch function in terms of AQL
struct SearchFunc {
  SearchFunc() = default;

  constexpr SearchFunc(aql::Variable const* var,
                       aql::AstNode const* node) noexcept
      : var{var}, node{node} {}

  constexpr bool operator==(SearchFunc const& rhs) const noexcept {
    return var == rhs.var && node == rhs.node;
  }

  aql::Variable const* var{};  // scorer variable
  aql::AstNode const* node{};  // scorer node
};

struct HashedSearchFunc : SearchFunc {
  HashedSearchFunc(aql::Variable const* var, aql::AstNode const* node) noexcept
      : SearchFunc{var, node}, hash{arangodb::iresearch::hash(node)} {}

  size_t hash;
};

struct SearchFuncHash {
  size_t operator()(HashedSearchFunc const& key) const noexcept {
    return key.hash;
  }
};

struct SearchFuncEqualTo {
  bool operator()(SearchFunc const& lhs, SearchFunc const& rhs) const {
    return arangodb::iresearch::equalTo(lhs.node, rhs.node);
  }
};

using DedupSearchFuncs =
    containers::FlatHashMap<HashedSearchFunc, aql::Variable const*,
                            SearchFuncHash, SearchFuncEqualTo>;

}  // namespace arangodb::iresearch
