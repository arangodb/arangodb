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

#include "SortRegister.h"

#include "Aql/AqlValue.h"
#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SortNode.h"

#if 0 // #ifdef USE_IRESEARCH
#include "IResearch/IResearchViewNode.h"
#include "IResearch/IResearchOrderFactory.h"

namespace {

int compareIResearchScores(
    irs::sort::prepared const* comparer,
    arangodb::transaction::Methods*,
    arangodb::aql::AqlValue const& lhs,
    arangodb::aql::AqlValue const& rhs
) {
  arangodb::velocypack::ValueLength tmp;

  auto const* lhsScore = reinterpret_cast<irs::byte_type const*>(lhs.slice().getString(tmp));
  auto const* rhsScore = reinterpret_cast<irs::byte_type const*>(rhs.slice().getString(tmp));

  if (comparer->less(lhsScore, rhsScore)) {
    return -1;
  }

  if (comparer->less(rhsScore, lhsScore)) {
    return 1;
  }

  return 0;
}

int compareAqlValues(
    irs::sort::prepared const*,
    arangodb::transaction::Methods* trx,
    arangodb::aql::AqlValue const& lhs,
    arangodb::aql::AqlValue const& rhs) {
  return arangodb::aql::AqlValue::Compare(trx, lhs, rhs, true);
}

}
#endif

namespace arangodb {
namespace aql {

// -----------------------------------------------------------------------------
// -- SECTION --                                                    SortRegister
// -----------------------------------------------------------------------------

SortRegister::SortRegister(
    RegisterId reg,
    SortElement const& element) noexcept
  : attributePath(element.attributePath),
    reg(reg),
    asc(element.ascending) {
}

#if 0 // #ifdef USE_IRESEARCH

void SortRegister::fill(
    ExecutionPlan const& execPlan,
    ExecutionNode::RegisterPlan const& regPlan,
    std::vector<SortElement> const& elements,
    std::vector<SortRegister>& sortRegisters
) {
  sortRegisters.reserve(elements.size());
  std::unordered_map<ExecutionNode const*, size_t> offsets(sortRegisters.capacity());

  irs::sort::ptr comparer;

  auto const& vars = regPlan.varInfo;
  for (auto const& p : elements) {
    auto const varId = p.var->id;
    auto const it = vars.find(varId);
    TRI_ASSERT(it != vars.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    sortRegisters.emplace_back(it->second.registerId, p, &compareAqlValues);

    auto const* setter = execPlan.getVarSetBy(varId);

    if (setter && ExecutionNode::ENUMERATE_IRESEARCH_VIEW == setter->getType()) {
      // sort condition is covered by `IResearchViewNode`

      auto const* viewNode = ExecutionNode::castTo<iresearch::IResearchViewNode const*>(setter);
      auto& offset = offsets[viewNode];
      auto* node = viewNode->sortCondition()[offset++].node;

      if (arangodb::iresearch::OrderFactory::comparer(&comparer, *node)) {
        auto& reg = sortRegisters.back();
        reg.scorer = comparer->prepare(); // set score comparer
        reg.comparator = &compareIResearchScores; // set comparator
      }
    }
  }
}

#else

void SortRegister::fill(
    ExecutionPlan const& /*execPlan*/,
    ExecutionNode::RegisterPlan const& regPlan,
    std::vector<SortElement> const& elements,
    std::vector<SortRegister>& sortRegisters
) {
  sortRegisters.reserve(elements.size());
  auto const& vars = regPlan.varInfo;

  for (auto const& p : elements) {
    auto const varId = p.var->id;
    auto const it = vars.find(varId);
    TRI_ASSERT(it != vars.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    sortRegisters.emplace_back(it->second.registerId, p);
  }
}

#endif // USE_IRESEARCH

} // aql
} // arangodb
