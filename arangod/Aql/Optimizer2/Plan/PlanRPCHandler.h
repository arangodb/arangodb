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
/// @author Heiko  Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Inspection/Types.h"

#include "Aql/Optimizer2/Types/Types.h"
#include "Aql/Optimizer2/Inspection/VPackInspection.h"

#include "Aql/Optimizer2/Plan/VerbosePlan.h"
#include "Aql/Optimizer2/Plan/ResultPlan.h"
#include "Aql/Optimizer2/Plan/QueryPostBody.h"

#include <fmt/core.h>

namespace arangodb::aql::optimizer2::plan {

using GeneratePlanCMD = arangodb::aql::optimizer2::plan::QueryPostBody;
using ExecutePlanCMD = arangodb::aql::optimizer2::plan::VerbosePlan;
using ResultPlan = arangodb::aql::optimizer2::plan::ResultPlan;

using PlanRPCVariant = std::variant<GeneratePlanCMD, ExecutePlanCMD>;

struct PlanRPC {
  PlanRPCVariant parsed;
};

template<class Inspector>
auto inspect(Inspector& f, PlanRPC& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x.parsed).unqualified().alternatives(
      insp::inlineType<GeneratePlanCMD>(), insp::inlineType<ExecutePlanCMD>());
}

template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

struct PlanRPCHandler {
  /*
   * If we receive:
   *  - QueryPostBody (GeneratePlanCMD), we will generate a detailed
   *    (verbose) query plan => "VerbosePlan"
   *  - VerbosePlan, we will try to execute the plan (ExecutePlanCMD) and
   *    return its result encapsulated into "ResultPlan"
   */

  auto process(velocypack::SharedSlice body) -> bool {
    auto res = deserializeWithStatus<PlanRPC>(body);
    if (!res.ok()) {
      // return std::move(res);
      return false;
    }
    auto cmd = res.get().parsed;

    std::visit(overloaded{[](GeneratePlanCMD const& plan) {
                            fmt::print("GenerateplaN");
                            return "generatePlan";
                          },
                          [](ExecutePlanCMD const& plan) {
                            fmt::print("execute eplaN");
                            return "executePlan";
                          },
                          [](auto) { return "not supported"; }},
               cmd);

    return true;
  }
};

}  // namespace arangodb::aql::optimizer2::plan