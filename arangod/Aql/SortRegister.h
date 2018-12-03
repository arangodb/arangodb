////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SORT_REGISTER_H
#define ARANGOD_AQL_SORT_REGISTER_H 1

#include "Aql/ExecutionNode.h"
#include "types.h"

#if 0 // #ifdef USE_IRESEARCH
#include "search/sort.hpp"
#endif

namespace arangodb {
namespace aql {

/// @brief sort element for block, consisting of register, sort direction,
/// and a possible attribute path to dig into the document
struct SortRegister {
#if 0 // #ifdef USE_IRESEARCH
  typedef int(*CompareFunc)(
    irs::sort::prepared const* scorer,
    transaction::Methods* trx,
    AqlValue const& lhs,
    AqlValue const& rhs
  );

  irs::sort::prepared::ptr scorer;
  CompareFunc comparator;
#endif
  std::vector<std::string> const& attributePath;
  RegisterId reg;
  bool asc;

  SortRegister(
    RegisterId reg,
    SortElement const& element
  ) noexcept;

#if 0 // #ifdef USE_IRESEARCH
  SortRegister(
      RegisterId reg,
      SortElement const& element,
      CompareFunc comparator) noexcept
    : SortRegister(reg, element) {
    this->comparator = comparator;
  }
#endif

  static void fill(
    ExecutionPlan const& /*execPlan*/,
    ExecutionNode::RegisterPlan const& regPlan,
    std::vector<SortElement> const& elements,
    std::vector<SortRegister>& sortRegisters
  );
}; // SortRegister

} // aql
} // arangodb

#endif // ARANGOD_AQL_SORT_REGISTER_H
