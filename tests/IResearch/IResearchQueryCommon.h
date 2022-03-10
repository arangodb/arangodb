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

#pragma once

#include "gtest/gtest.h"

#include "../3rdParty/iresearch/tests/tests_config.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/norm.hpp"
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

inline auto GetLinkVersions() noexcept {
  return testing::Values(arangodb::iresearch::LinkVersion::MIN,
                         arangodb::iresearch::LinkVersion::MAX);
}

class IResearchQueryTest
    : public ::testing::TestWithParam<arangodb::iresearch::LinkVersion>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchQueryTest() : server(false) {
    arangodb::tests::init(true);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();

    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(
        testDBInfo(server.server()),
        _vocbase);  // required for IResearchAnalyzerFeature::emplace(...)

    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(
        *_vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        unused);
    unused = nullptr;

    auto res = analyzers.emplace(
        result, "testVocbase::test_analyzer", "TestAnalyzer",
        VPackParser::fromJson("\"abc\"")->slice(),
        arangodb::iresearch::Features(
            {}, irs::IndexFeatures::FREQ |
                    irs::IndexFeatures::POS));  // required for PHRASE
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result, "testVocbase::test_csv_analyzer", "TestDelimAnalyzer",
        VPackParser::fromJson("\",\"")->slice());  // cache analyzer
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result, "testVocbase::text_en", "text",
        VPackParser::fromJson(
            "{ \"locale\": \"en.UTF-8\", \"stopwords\": [ ] }")
            ->slice(),
        arangodb::iresearch::Features{
            arangodb::iresearch::FieldFeatures::NORM,
            irs::IndexFeatures::FREQ |
                irs::IndexFeatures::POS});  // cache analyzer
    EXPECT_TRUE(res.ok());

    auto sysVocbase =
        server.getFeature<arangodb::SystemDatabaseFeature>().use();
    arangodb::methods::Collections::createSystem(
        *sysVocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        unused);
    unused = nullptr;

    res =
        analyzers.emplace(result, "_system::test_analyzer", "TestAnalyzer",
                          VPackParser::fromJson("\"abc\"")->slice(),
                          arangodb::iresearch::Features{
                              irs::IndexFeatures::FREQ |
                              irs::IndexFeatures::POS});  // required for PHRASE

    res = analyzers.emplace(
        result, "_system::ngram_test_analyzer13", "ngram",
        VPackParser::fromJson("{\"min\":1, \"max\":3, \"streamType\":\"utf8\", "
                              "\"preserveOriginal\":false}")
            ->slice(),
        arangodb::iresearch::Features{
            irs::IndexFeatures::FREQ |
            irs::IndexFeatures::POS});  // required for PHRASE

    res = analyzers.emplace(
        result, "_system::ngram_test_analyzer2", "ngram",
        VPackParser::fromJson("{\"min\":2, \"max\":2, \"streamType\":\"utf8\", "
                              "\"preserveOriginal\":false}")
            ->slice(),
        arangodb::iresearch::Features{
            irs::IndexFeatures::FREQ |
            irs::IndexFeatures::POS});  // required for PHRASE

    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result, "_system::test_csv_analyzer", "TestDelimAnalyzer",
        VPackParser::fromJson("\",\"")->slice());  // cache analyzer
    EXPECT_TRUE(res.ok());

    auto& functions = server.getFeature<arangodb::aql::AqlFunctionFeature>();
    // register fake non-deterministic function in order to suppress
    // optimizations
    functions.add(arangodb::aql::Function{
        "_NONDETERM_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // register fake non-deterministic function in order to suppress
    // optimizations
    functions.add(arangodb::aql::Function{
        "_FORWARD_", ".",
        arangodb::aql::Function::makeFlags(
            // fake deterministic
            arangodb::aql::Function::Flags::Deterministic,
            arangodb::aql::Function::Flags::Cacheable,
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // external function names must be registred in upper-case
    // user defined functions have ':' in the external function name
    // function arguments string format:
    // requiredArg1[,requiredArg2]...[|optionalArg1[,optionalArg2]...]
    arangodb::aql::Function customScorer(
        "CUSTOMSCORER", ".|+",
        arangodb::aql::Function::makeFlags(
            arangodb::aql::Function::Flags::Deterministic,
            arangodb::aql::Function::Flags::Cacheable,
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        nullptr);
    arangodb::iresearch::addFunction(functions, customScorer);

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(
        dbPathFeature);  // ensure test data is stored in a unique directory
  }

  TRI_vocbase_t& vocbase() {
    TRI_ASSERT(_vocbase != nullptr);
    return *_vocbase;
  }

  arangodb::iresearch::LinkVersion linkVersion() const noexcept {
    return GetParam();
  }
};  // IResearchQueryTest
