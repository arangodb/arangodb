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
/// @author Max Neunhoeffer
/// @author Michael Hackstein
/// @author Tobias Gödderz
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "RegisterPlan.h"

#include "RegisterPlan.tpp"

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

template<typename T>
std::ostream& aql::operator<<(std::ostream& os, const RegisterPlanT<T>& r) {
  // level -> variable, info
  std::map<unsigned int, std::map<VariableId, VarInfo>> frames;

  for (auto [id, info] : r.varInfo) {
    frames[info.depth][id] = info;
  }

  for (auto [depth, vars] : frames) {
    os << "depth " << depth << std::endl;
    os << "------------------------------------" << std::endl;

    for (auto [id, info] : vars) {
      os << "id = " << id << " register = " << info.registerId.value()
         << std::endl;
    }
  }
  return os;
}

template struct aql::RegisterPlanT<ExecutionNode>;
template struct aql::RegisterPlanWalkerT<ExecutionNode>;
template std::ostream& aql::operator<<<ExecutionNode>(
    std::ostream& os, const RegisterPlanT<ExecutionNode>& r);
