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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/types.h"

#include <string>
#include <vector>

namespace arangodb::aql {
class ExecutionNode;
class ExecutionPlan;
template<typename T>
struct RegisterPlanT;
using RegisterPlan = RegisterPlanT<ExecutionNode>;
struct SortElement;

/// @brief sort element for block, consisting of register, sort direction,
/// and a possible attribute path to dig into the document
struct SortRegister {
  SortRegister(SortRegister const&) =
      delete;  // we can not copy the iresearch scorer
  SortRegister(SortRegister&&) = default;

  std::vector<std::string> const& attributePath;
  RegisterId reg;
  bool asc;

  SortRegister(RegisterId reg, SortElement const& element) noexcept;

  static void fill(ExecutionPlan const& /*execPlan*/,
                   RegisterPlan const& regPlan,
                   std::vector<SortElement> const& elements,
                   std::vector<SortRegister>& sortRegisters);
};  // SortRegister

}  // namespace arangodb::aql
