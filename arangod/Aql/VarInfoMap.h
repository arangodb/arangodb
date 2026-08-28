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

#pragma once

#include "Aql/RegisterId.h"
#include "Aql/types.h"

#include <unordered_map>
#include <utility>
// TODO(MBkkt) Try to use FlatHashMap to achieve better
//  lookup performance on query execution
// #include "Containers/FlatHashMap.h"

namespace arangodb::aql {

struct Variable;

/// @brief where a variable lives: the depth at which it is assigned a
/// register, and that register.
struct VarInfo {
  unsigned int depth{0};
  RegisterId registerId;

  VarInfo() = default;
  VarInfo(unsigned int depth, RegisterId registerId);
};

using VarInfoMap = std::unordered_map<VariableId, VarInfo>;

/// @brief Resolves variables to registers for rows of one specific depth.
///
/// "Depth" here is the register-planning notion: a run of adjacent execution
/// nodes that share one block width and one register numbering. Every register
/// lookup is really a question about a specific row, and a row always belongs
/// to exactly one depth -- the rows a node reads sit at the depth of its
/// dependency, the rows it writes sit at its own depth.
///
/// Hand this around instead of a bare VarInfoMap so that the depth travels
/// with the map and a caller cannot resolve against the wrong numbering.
/// Obtain one from ExecutionNode::inputRegisterResolver() /
/// outputRegisterResolver().
class RegisterResolver {
 public:
  enum class Status {
    Ok,
    /// @brief the variable was never assigned a register
    UnknownVariable,
    /// @brief the variable is assigned a register, but only becomes available
    /// at a greater depth than the one being resolved for
    NotYetAssigned,
  };

  RegisterResolver(VarInfoMap const& varInfo, unsigned int depth) noexcept
      : _varInfo(&varInfo), _depth(depth) {}

  /// @brief register holding `variable` in rows of this depth. The variable
  /// must have a register and must be available here.
  RegisterId registerFor(Variable const& variable) const;
  RegisterId registerFor(Variable const* variable) const;

  /// @brief register holding the variable, or an invalid RegisterId if it has
  /// no register or is not available at this depth.
  RegisterId tryRegisterFor(VariableId id) const noexcept;

  /// @brief full lookup result, for callers that need to tell the two failure
  /// modes apart.
  std::pair<RegisterId, Status> lookup(VariableId id) const noexcept;

  unsigned int depth() const noexcept { return _depth; }

  VarInfoMap const& varInfo() const noexcept { return *_varInfo; }

 private:
  VarInfoMap const* _varInfo;
  unsigned int _depth;
};

}  // namespace arangodb::aql
