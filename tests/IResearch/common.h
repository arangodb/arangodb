////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef TESTS_IRESEARCH__COMMON_H
#define TESTS_IRESEARCH__COMMON_H 1

#include "Aql/AstNode.h"
#include "Aql/Query.h"
#include "Basics/StaticStrings.h"
#include "VocBase/VocbaseInfo.h"
#include "VocBase/vocbase.h"

#include <string>
#include <vector>

#undef NO_INLINE  // to avoid GCC warning
#include <search/filter.hpp>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief a TRI_vocbase_t that will call shutdown() on deallocation
///        to force deallocation of dropped collections
////////////////////////////////////////////////////////////////////////////////
struct Vocbase : public TRI_vocbase_t {
  template <typename... Args>
  Vocbase(Args&&... args) : TRI_vocbase_t(std::forward<Args>(args)...) {}
  ~Vocbase() { shutdown(); }
};

namespace v8 {
class Isolate;  // forward declaration
}

namespace arangodb {

class DatabasePathFeature;  // forward declaration

namespace application_features {
class ApplicationServer;
}

namespace aql {
class ExpressionContext;
}

namespace iresearch {
class ByExpression;
}

namespace tests {

extern std::string const AnalyzerCollectionName;

extern std::string testResourceDir;

void init(bool withICU = false);

template <typename T, typename U>
std::shared_ptr<T> scopedPtr(T*& ptr, U* newValue) {
  auto* location = &ptr;
  auto* oldValue = ptr;
  ptr = newValue;
  return std::shared_ptr<T>(oldValue, [location](T* p) -> void { *location = p; });
}

// @Note: once V8 is initialized all 'CATCH' errors will result in SIGILL
void v8Init();

v8::Isolate* v8Isolate();

bool assertRules(TRI_vocbase_t& vocbase, std::string const& queryString,
                 std::vector<int> expectedRulesIds,
                 std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                 std::string const& optionsString = "{}");

arangodb::aql::QueryResult explainQuery(
    TRI_vocbase_t& vocbase, std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& optionsString = "{}");

arangodb::aql::QueryResult executeQuery(
    TRI_vocbase_t& vocbase, std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& optionsString = "{}");

std::unique_ptr<arangodb::aql::ExecutionPlan> planFromQuery(
    TRI_vocbase_t& vocbase, std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& optionsString = "{}");

std::unique_ptr<arangodb::aql::Query> prepareQuery(
    TRI_vocbase_t& vocbase, std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& optionsString = "{}");

uint64_t getCurrentPlanVersion(arangodb::application_features::ApplicationServer&);

void setDatabasePath(arangodb::DatabasePathFeature& feature);

#define EXPECT_EQUAL_SLICES_STRINGIFY(x) #x
#define EXPECT_EQUAL_SLICES_EXPANDER(leftSlice, rightSlice, file, line) \
  arangodb::tests::expectEqualSlices_(leftSlice, rightSlice,            \
                                      file ":" EXPECT_EQUAL_SLICES_STRINGIFY(line))
#define EXPECT_EQUAL_SLICES(leftSlice, rightSlice) \
  EXPECT_EQUAL_SLICES_EXPANDER(leftSlice, rightSlice, __FILE__, __LINE__)
void expectEqualSlices_(const velocypack::Slice& lhs,
                        const velocypack::Slice& rhs, const char* where);

}  // namespace tests
}  // namespace arangodb

std::string mangleType(std::string name);
std::string mangleAnalyzer(std::string name);
std::string mangleBool(std::string name);
std::string mangleNull(std::string name);
std::string mangleNumeric(std::string name);
std::string mangleString(std::string name, std::string_view suffix);
std::string mangleStringIdentity(std::string name);

inline arangodb::aql::AstNode* defaultExpressionExtractor(arangodb::aql::AstNode* root) {
  return root->getMember(0);
}

inline arangodb::aql::AstNode* wrappedExpressionExtractor(arangodb::aql::AstNode* root) {
  return defaultExpressionExtractor(root)->getMember(0)->getMember(0);
}

void assertExpressionFilter(
    TRI_vocbase_t& vocbase, std::string const& queryString,
    irs::boost_t boost = irs::no_boost(),
    std::function<arangodb::aql::AstNode*(arangodb::aql::AstNode*)> const& expressionExtractor = &defaultExpressionExtractor,
    std::string const& refName = "d");

void assertFilterBoost(irs::filter const& expected, irs::filter const& actual);

void assertFilterOptimized(TRI_vocbase_t& vocbase, std::string const& queryString,
                           irs::filter const& expectedFilter,
                           arangodb::aql::ExpressionContext* exprCtx = nullptr,
                           std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr);

void assertFilter(TRI_vocbase_t& vocbase, bool parseOk, bool execOk,
                  std::string const& queryString, irs::filter const& expected,
                  arangodb::aql::ExpressionContext* exprCtx = nullptr,
                  std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                  std::string const& refName = "d");

void assertFilterSuccess(TRI_vocbase_t& vocbase, std::string const& queryString,
                         irs::filter const& expected,
                         arangodb::aql::ExpressionContext* exprCtx = nullptr,
                         std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                         std::string const& refName = "d");

void assertFilterFail(TRI_vocbase_t& vocbase, std::string const& queryString,
                      arangodb::aql::ExpressionContext* exprCtx = nullptr,
                      std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                      std::string const& refName = "d");

void assertFilterExecutionFail(TRI_vocbase_t& vocbase, std::string const& queryString,
                               arangodb::aql::ExpressionContext* exprCtx = nullptr,
                               std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                               std::string const& refName = "d");

void assertFilterParseFail(TRI_vocbase_t& vocbase, std::string const& queryString,
                           std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr);

void buildActualFilter(TRI_vocbase_t& vocbase, std::string const& queryString,
                       irs::filter& actual,
                       arangodb::aql::ExpressionContext* exprCtx = nullptr,
                       std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                       std::string const& refName = "d");

inline VPackBuilder dbArgsBuilder(std::string const& name = "_system") {
  VPackOptions options;
  VPackBuilder builder(&options);
  builder.openObject();
  builder.add("name", VPackValue(name));
  builder.add("sharding", VPackValue(std::string{}));
  builder.add("replicationFactor", VPackValue(1));
  builder.close();
  return builder;
}

arangodb::CreateDatabaseInfo createInfo(arangodb::application_features::ApplicationServer& server,
                                        std::string const& name, uint64_t id);
arangodb::CreateDatabaseInfo systemDBInfo(
    arangodb::application_features::ApplicationServer& server,
    std::string const& name = arangodb::StaticStrings::SystemDatabase, uint64_t id = 1);
arangodb::CreateDatabaseInfo testDBInfo(arangodb::application_features::ApplicationServer& server,
                                        std::string const& name = "testVocbase",
                                        uint64_t id = 2);
arangodb::CreateDatabaseInfo unknownDBInfo(
    arangodb::application_features::ApplicationServer& server,
    std::string const& name = "unknownVocbase", uint64_t id = 3);

#endif
