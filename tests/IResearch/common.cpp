////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "common.h"
#include "Agency/AgencyComm.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Ast.h"
#include "VocBase/KeyGenerator.h"
#include "RestServer/QueryRegistryFeature.h"
#include "IResearch/VelocyPackHelper.h"
#include "IResearch/ExpressionFilter.h"
#include "tests/Basics/icu-helper.h"

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include <unordered_set>

extern const char* ARGV0; // defined in main.cpp

NS_LOCAL

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(tests)

void init(bool withICU /*= false*/) {
  arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
}

bool assertRules(
    TRI_vocbase_t& vocbase,
    std::string const& queryString,
    std::vector<int> expectedRulesIds,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /* = nullptr */
) {
  std::unordered_set<std::string> expectedRules;
  for (auto ruleId : expectedRulesIds) {
    expectedRules.emplace(arangodb::aql::OptimizerRulesFeature::translateRule(ruleId));
  }

  auto options = arangodb::velocypack::Parser::fromJson(
//    "{ \"tracing\" : 1 }"
    "{ }"
  );

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    bindVars,
    options,
    arangodb::aql::PART_MAIN
  );

  auto const res = query.explain();

  if (res.result) {
    auto const explanation = res.result->slice();

    arangodb::velocypack::ArrayIterator rules(explanation.get("rules"));

    for (auto const rule : rules) {
      auto const strRule = arangodb::iresearch::getStringRef(rule);
      expectedRules.erase(strRule);
    }
  }

  return expectedRules.empty();
}

arangodb::aql::QueryResult executeQuery(
    TRI_vocbase_t& vocbase,
    std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
    bool waitForSync /* = false*/
) {
  auto options = arangodb::velocypack::Parser::fromJson(
//    "{ \"tracing\" : 1 }"
    waitForSync ? "{ \"waitForSync\": true }" : "{ }"
  );

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    bindVars,
    options,
    arangodb::aql::PART_MAIN
  );

  arangodb::aql::QueryResult result;
  while (true) {
    auto state = query.execute(arangodb::QueryRegistryFeature::QUERY_REGISTRY, result);
    if (state == arangodb::aql::ExecutionState::WAITING) {
      query.tempWaitForAsyncResponse();
    } else {
      break;
    }
  }

  return result;
}

std::unique_ptr<arangodb::aql::ExecutionPlan> planFromQuery(
  TRI_vocbase_t& vocbase,
  std::string const& queryString,
  std::shared_ptr<arangodb::velocypack::Builder> bindVars /* = nullptr */
) {
  auto options = arangodb::velocypack::Parser::fromJson(
//    "{ \"tracing\" : 1 }"
    "{ }"
  );

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    nullptr,
    options,
    arangodb::aql::PART_MAIN
  );

  auto result = query.parse();

  if (result.code != TRI_ERROR_NO_ERROR || !query.ast()) {
    return nullptr;
  }

  return std::unique_ptr<arangodb::aql::ExecutionPlan>(
    arangodb::aql::ExecutionPlan::instantiateFromAst(query.ast())
  );
}

uint64_t getCurrentPlanVersion() {
  auto const result = arangodb::AgencyComm().getValues("Plan");
  auto const planVersionSlice = result.slice()[0].get(
    { arangodb::AgencyCommManager::path(), "Plan", "Version" }
  );
  return planVersionSlice.getNumber<uint64_t>();
}

NS_END // tests
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
