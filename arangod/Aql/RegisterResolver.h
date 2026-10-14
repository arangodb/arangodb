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

#include <utility>
#include "Containers/FlatHashMap.h"

namespace arangodb::aql {

struct Variable;

struct VarInfo {
  unsigned int depth{0};
  RegisterId registerId;

  VarInfo() = default;
  VarInfo(unsigned int depth, RegisterId registerId);
};

using VarInfoMap = containers::FlatHashMap<VariableId, VarInfo>;

/// @brief Resolves variables to registers for rows of one specific depth.
class RegisterResolver {
 public:
  enum class Status {
    Ok,
    UnknownVariable,
    NotYetAssigned,
  };

  RegisterResolver(VarInfoMap const& varInfo, unsigned int depth) noexcept
      : _varInfo(&varInfo), _depth(depth) {}

  /// @brief register holding `variable` in rows of this depth. The variable
  /// must have a register and must be available here.
  RegisterId resolve(Variable const& variable) const;
  RegisterId resolve(Variable const* variable) const;

  /// @brief register holding the variable, or an invalid RegisterId if it has
  /// no register or is not available at this depth.
  RegisterId tryResolve(VariableId id) const noexcept;

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
