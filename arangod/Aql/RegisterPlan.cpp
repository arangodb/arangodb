////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "RegisterPlan.h"

#include "RegisterPlan.tpp"

#include "Aql/Variable.h"

using namespace arangodb;
using namespace arangodb::aql;

// Requires RegisterPlan to be defined
VarInfo::VarInfo(unsigned int depth, RegisterId registerId)
    : depth(depth), registerId(registerId) {
  if (!registerId.isValid()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_RESOURCE_LIMIT,
        std::format("too many registers ({}) needed for AQL query",
                    registerId.value()));
  }
}

RegisterId RegisterResolver::registerFor(Variable const& variable) const {
  auto [reg, status] = lookup(variable.id);
  TRI_ASSERT(status == Status::Ok)
      << "variable " << variable.name << " (" << variable.id << ") "
      << (status == Status::UnknownVariable ? "has no register assigned"
                                            : "is not available here")
      << ", resolved for depth " << _depth;
  TRI_ASSERT(reg.isValid());
  return reg;
}

RegisterId RegisterResolver::registerFor(Variable const* variable) const {
  TRI_ASSERT(variable != nullptr);
  return registerFor(*variable);
}

RegisterId RegisterResolver::tryRegisterFor(VariableId id) const noexcept {
  return lookup(id).first;
}

auto RegisterResolver::lookup(VariableId id) const noexcept
    -> std::pair<RegisterId, Status> {
  auto it = _varInfo->find(id);
  if (it == _varInfo->end()) {
    return {RegisterId::makeInvalid(), Status::UnknownVariable};
  }
  if (it->second.depth > _depth) {
    return {RegisterId::makeInvalid(), Status::NotYetAssigned};
  }
  return {it->second.registerId, Status::Ok};
}

MissingVariablesException::MissingVariablesException(
    Variable const* variable, ExecutionNode const* node,
    basics::SourceLocation location)
    : Exception(TRI_ERROR_INTERNAL,
                createMissingVariablesExceptionMessage(variable, node),
                location),
      _variable(variable),
      _node(node) {}

auto MissingVariablesException::variable() const noexcept -> Variable const* {
  return _variable;
}

auto MissingVariablesException::node() const noexcept -> ExecutionNode const* {
  return _node;
}

template struct aql::RegisterPlanT<ExecutionNode>;
template struct aql::RegisterPlanWalkerT<ExecutionNode>;
template std::ostream& aql::operator<<<ExecutionNode>(
    std::ostream& os, const RegisterPlanT<ExecutionNode>& r);
