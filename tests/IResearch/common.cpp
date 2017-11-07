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
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "IResearch/VelocyPackHelper.h"

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include <unordered_set>

extern char* ARGV0; // defined in main.cpp

////////////////////////////////////////////////////////////////////////////////
/// @brief there can be at most one ArangoGlobalConetxt instance because each
///        instance creation calls TRIAGENS_REST_INITIALIZE() which in tern
///        calls TRI_InitializeError() which cannot be called multiple times
////////////////////////////////////////////////////////////////////////////////
struct singleton_t {
  // required for DatabasePathFeature and TRI_InitializeErrorMessages()
  arangodb::ArangoGlobalContext ctx;

  // required for creation of V* Isolate instances
  arangodb::V8PlatformFeature v8PlatformFeature;

  singleton_t()
    : ctx(1, &ARGV0, "."),
      v8PlatformFeature(nullptr) {
    v8PlatformFeature.start(); // required for createIsolate()
  }

  ~singleton_t() {
    v8PlatformFeature.unprepare();
  }
};

static singleton_t* SINGLETON = nullptr;

namespace arangodb {
namespace tests {

void init() {
  static singleton_t singleton;
  SINGLETON = &singleton;
}

v8::Isolate* v8Isolate() {
  return SINGLETON ? SINGLETON->v8PlatformFeature.createIsolate() : nullptr;
}

bool assertRules(
    TRI_vocbase_t& vocbase,
    const std::string& queryString,
    std::vector<int> expectedRulesIds
) {
  std::unordered_set<std::string> expectedRules;
  for (auto ruleId : expectedRulesIds) {
    expectedRules.emplace(arangodb::aql::OptimizerRulesFeature::translateRule(ruleId));
  }

  std::shared_ptr<arangodb::velocypack::Builder> bindVars;

  auto options = arangodb::velocypack::Parser::fromJson(
//    "{ \"tracing\" : 1 }"
    "{ }"
  );

  arangodb::aql::Query query(
    false, &vocbase, arangodb::aql::QueryString(queryString),
    bindVars, options,
    arangodb::aql::PART_MAIN
  );

  auto const res = query.explain();
  auto const explanation = res.result->slice();

  arangodb::velocypack::ArrayIterator rules(explanation.get("rules"));

//  if (expectedRules.size() != rules.size()) {
//    return false;
//  }

  for (auto const rule : rules) {
    auto const strRule = arangodb::iresearch::getStringRef(rule);
    expectedRules.erase(strRule);
  }

  return expectedRules.empty();
}

arangodb::aql::QueryResult executeQuery(
    TRI_vocbase_t& vocbase,
    const std::string& queryString
) {
  std::shared_ptr<arangodb::velocypack::Builder> bindVars;

  auto options = arangodb::velocypack::Parser::fromJson(
//    "{ \"tracing\" : 1 }"
    "{ }"
  );

  arangodb::aql::Query query(
    false, &vocbase, arangodb::aql::QueryString(queryString),
    bindVars, options,
    arangodb::aql::PART_MAIN
  );

  return query.execute(arangodb::QueryRegistryFeature::QUERY_REGISTRY);
}

}
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
