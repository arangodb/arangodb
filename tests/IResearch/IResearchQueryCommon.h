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
#include "gtest/gtest.h"

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

#include "3rdParty/iresearch/tests/tests_config.hpp"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

class IResearchQueryTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchQueryTest() : server(false) {
    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // suppress INFO {cluster} Starting up with role SINGLE
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress WARNING DefaultCustomTypeHandler called

    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(1, "testVocbase", _vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    arangodb::methods::Collections::createSystem(*_vocbase, arangodb::tests::AnalyzerCollectionName,
                                                 false);

    auto res =
        analyzers.emplace(result, "testVocbase::test_analyzer", "TestAnalyzer",
                          VPackParser::fromJson("\"abc\"")->slice(),
                          irs::flags{irs::frequency::type(), irs::position::type()}  // required for PHRASE
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
        {irs::frequency::type(), irs::norm::type(), irs::position::type()});  // cache analyzer
    EXPECT_TRUE(res.ok());

    auto sysVocbase = server.getFeature<arangodb::SystemDatabaseFeature>().use();
    arangodb::methods::Collections::createSystem(*sysVocbase, arangodb::tests::AnalyzerCollectionName,
                                                 false);

    res = analyzers.emplace(result, "_system::test_analyzer", "TestAnalyzer",
                            VPackParser::fromJson("\"abc\"")->slice(),
                            irs::flags{irs::frequency::type(), irs::position::type()}  // required for PHRASE
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
            arangodb::aql::Function::Flags::CanRunOnDBServer),
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
            arangodb::aql::Function::Flags::CanRunOnDBServer),
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
                                           arangodb::aql::Function::Flags::CanRunOnDBServer));
    arangodb::iresearch::addFunction(functions, customScorer);

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  }

  ~IResearchQueryTest() {
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
  }

  TRI_vocbase_t& vocbase() {
    TRI_ASSERT(_vocbase != nullptr);
    return *_vocbase;
  }
};  // IResearchQueryTest
