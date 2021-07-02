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

#ifndef ARANGOD_AQL_IRESEARCH_QUERY_COMMON_H
#define ARANGOD_AQL_IRESEARCH_QUERY_COMMON_H

#include "gtest/gtest.h"

#include "3rdParty/iresearch/tests/tests_config.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"

#include "Aql/AqlFunctionFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "VocBase/Methods/Collections.h"

class IResearchQueryTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchQueryTest() : server(false) {
    arangodb::tests::init(true);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(testDBInfo(server.server()), _vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    
    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::methods::Collections::createSystem(*_vocbase, arangodb::tests::AnalyzerCollectionName,
                                                 false, unused);
    unused = nullptr;

    auto res =
        analyzers.emplace(result, "testVocbase::test_analyzer", "TestAnalyzer",
                          VPackParser::fromJson("\"abc\"")->slice(),
                          irs::flags{irs::type<irs::frequency>::get(), irs::type<irs::position>::get()}  // required for PHRASE
        );  // cache analyzer
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(result, "testVocbase::test_csv_analyzer",
                            "TestDelimAnalyzer",
                            VPackParser::fromJson("\",\"")->slice());  // cache analyzer
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result, "testVocbase::text_en", "text",
        VPackParser::fromJson(
            "{ \"locale\": \"en.UTF-8\", \"stopwords\": [ ] }")
            ->slice(),
        {irs::type<irs::frequency>::get(), irs::type<irs::norm>::get(), irs::type<irs::position>::get()});  // cache analyzer
    EXPECT_TRUE(res.ok());

    auto sysVocbase = server.getFeature<arangodb::SystemDatabaseFeature>().use();
    arangodb::methods::Collections::createSystem(*sysVocbase, arangodb::tests::AnalyzerCollectionName,
                                                 false, unused);
    unused = nullptr;

    res = analyzers.emplace(result, "_system::test_analyzer", "TestAnalyzer",
                            VPackParser::fromJson("\"abc\"")->slice(),
                            irs::flags{irs::type<irs::frequency>::get(), irs::type<irs::position>::get()}  // required for PHRASE
    );  // cache analyzer
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(result, "_system::test_csv_analyzer",
                            "TestDelimAnalyzer",
                            VPackParser::fromJson("\",\"")->slice());  // cache analyzer
    EXPECT_TRUE(res.ok());

    auto& functions = server.getFeature<arangodb::aql::AqlFunctionFeature>();
    // register fake non-deterministic function in order to suppress optimizations
    functions.add(arangodb::aql::Function{
        "_NONDETERM_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster, 
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // register fake non-deterministic function in order to suppress optimizations
    functions.add(arangodb::aql::Function{
        "_FORWARD_", ".",
        arangodb::aql::Function::makeFlags(
            // fake deterministic
            arangodb::aql::Function::Flags::Deterministic, arangodb::aql::Function::Flags::Cacheable,
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // external function names must be registred in upper-case
    // user defined functions have ':' in the external function name
    // function arguments string format: requiredArg1[,requiredArg2]...[|optionalArg1[,optionalArg2]...]
    arangodb::aql::Function customScorer(
        "CUSTOMSCORER", ".|+",
        arangodb::aql::Function::makeFlags(arangodb::aql::Function::Flags::Deterministic,
                                           arangodb::aql::Function::Flags::Cacheable,
                                           arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
                                           arangodb::aql::Function::Flags::CanRunOnDBServerOneShard), 
        nullptr);
    arangodb::iresearch::addFunction(functions, customScorer);

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  }

  TRI_vocbase_t& vocbase() {
    TRI_ASSERT(_vocbase != nullptr);
    return *_vocbase;
  }
};  // IResearchQueryTest

#endif // ARANGOD_AQL_IRESEARCH_QUERY_COMMON_H
