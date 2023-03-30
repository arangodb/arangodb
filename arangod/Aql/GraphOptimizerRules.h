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

#pragma once

#include <optional>
#include <memory>
#include <variant>

namespace arangodb {
namespace aql {

class Ast;
struct AstNode;
class Optimizer;
class ExecutionPlan;
struct OptimizerRule;
struct Variable;

struct PathVariableAccess {
  struct AllAccess {};
  enum class AccessType { VERTEX, EDGE };

  std::variant<int64_t, AllAccess> index{0};
  AccessType type{AccessType::VERTEX};
  AstNode* parentOfReplace{nullptr};
  size_t replaceIdx{0};

  auto isLast() const noexcept -> bool;

  auto getDepth() const noexcept -> int64_t;

  auto isAllAccess() const noexcept -> bool;

  auto isEdgeAccess() const noexcept -> bool;

  auto isVertexAccess() const noexcept -> bool;
};

auto maybeExtractPathAccess(Ast* ast, Variable const* pathVar, AstNode* parent,
                            size_t testIndex)
    -> std::optional<PathVariableAccess>;

/// @brief replaces the last element on path access with the direct output of
/// vertex/edge
void replaceLastAccessOnGraphPathRule(Optimizer* opt,
                                  std::unique_ptr<ExecutionPlan> plan,
                                  OptimizerRule const&);

}  // namespace aql
}  // namespace arangodb
