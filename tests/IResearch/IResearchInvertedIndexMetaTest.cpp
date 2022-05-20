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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////


#include "gtest/gtest.h"

#include "IResearch/common.h"
#include "IResearch\IResearchInvertedIndexMeta.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "VocBase/Methods/Collections.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"

using namespace arangodb;
using namespace arangodb::iresearch;

class IResearchInvertedIndexMetaTest
    : public ::testing::Test,
      public tests::LogSuppressor<arangodb::Logger::AGENCYCOMM,
                                  LogLevel::FATAL>,
      public tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                  LogLevel::ERR> {
 protected:
  tests::mocks::MockAqlServer server;

  IResearchInvertedIndexMetaTest() {
    tests::init();

    auto& dbFeature = server.getFeature<DatabaseFeature>();
    auto sysvocbase =
        dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
    std::shared_ptr<LogicalCollection> unused;
    OperationOptions options(ExecContext::current());
    methods::Collections::createSystem(
        *sysvocbase, options, tests::AnalyzerCollectionName, false, unused);
    unused = nullptr;

    TRI_vocbase_t* vocbase;
    dbFeature.createDatabase(
        testDBInfo(server.server()),
        vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    methods::Collections::createSystem(
        *vocbase, options, tests::AnalyzerCollectionName, false, unused);

    auto& analyzers = server.getFeature<IResearchAnalyzerFeature>();
    IResearchAnalyzerFeature::EmplaceResult result;

    analyzers.emplace(
        result, "testVocbase::empty", "empty",
        VPackParser::fromJson("{ \"args\": \"de\" }")->slice(),
        Features(irs::IndexFeatures::FREQ));  // cache the 'empty' analyzer for
                                              // 'testVocbase'
  }
};

TEST_F(IResearchInvertedIndexMetaTest, test_defaults) {
  arangodb::iresearch::IResearchInvertedIndexMeta meta;

  ASSERT_EQ(0, meta._analyzerDefinitions.size());
  ASSERT_TRUE(true == meta._fields.empty());
  ASSERT_TRUE(meta._sort.empty());
  ASSERT_TRUE(meta._storedValues.empty());
  ASSERT_EQ(meta._sortCompression, irs::type<irs::compression::lz4>::id());
  ASSERT_TRUE(meta._analyzerDefinitions.empty());
  ASSERT_FALSE(meta.dense());
  ASSERT_EQ(meta._version,
            static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MAX));
  ASSERT_EQ(meta. _consistency, Consistency::kEventual);
  ASSERT_TRUE(meta._defaultAnalyzerName.empty());
  ASSERT_FALSE(meta._features);
}

TEST_F(IResearchInvertedIndexMetaTest, test_readDefaults) {
  auto json = VPackParser::fromJson(R"({
    "fields": ["dummy"]
  })");
  // without active vobcase
  {
    arangodb::iresearch::IResearchInvertedIndexMeta meta;
    std::string errorString;
    ASSERT_TRUE(meta.init(server.server(), json->slice(), false, errorString,
                          irs::string_ref::NIL));
    ASSERT_TRUE(errorString.empty());
    ASSERT_EQ(0, meta._analyzerDefinitions.size());
    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ("dummy", meta._fields.front().toString());
    ASSERT_TRUE(meta._sort.empty());
    ASSERT_TRUE(meta._storedValues.empty());
    ASSERT_EQ(meta._sortCompression, irs::type<irs::compression::lz4>::id());
    ASSERT_TRUE(meta._analyzerDefinitions.empty());
    ASSERT_FALSE(meta.dense());
    ASSERT_EQ(meta._version,
              static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MAX));
    ASSERT_EQ(meta._consistency, Consistency::kEventual);
    ASSERT_TRUE(meta._defaultAnalyzerName.empty());
    ASSERT_FALSE(meta._features);
  }
  // with active vobcase
  {
    arangodb::iresearch::IResearchInvertedIndexMeta meta;
    std::string errorString;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    ASSERT_TRUE(meta.init(server.server(), json->slice(), false, errorString,
                          irs::string_ref(vocbase.name())));
    ASSERT_TRUE(errorString.empty());
    ASSERT_EQ(0, meta._analyzerDefinitions.size());
    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ("dummy", meta._fields.front().toString());
    ASSERT_TRUE(meta._sort.empty());
    ASSERT_TRUE(meta._storedValues.empty());
    ASSERT_EQ(meta._sortCompression, irs::type<irs::compression::lz4>::id());
    ASSERT_TRUE(meta._analyzerDefinitions.empty());
    ASSERT_FALSE(meta.dense());
    ASSERT_EQ(meta._version,
              static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MAX));
    ASSERT_EQ(meta._consistency, Consistency::kEventual);
    ASSERT_TRUE(meta._defaultAnalyzerName.empty());
    ASSERT_FALSE(meta._features);
  }
}

TEST_F(IResearchInvertedIndexMetaTest, test_readCustomizedValues) {
  // without active vobcase
  auto json = VPackParser::fromJson(R"({
    "fields": [
       "simple",
        {
          "expression": "RETURN MERGE(@param, {foo: 'bar'}) ",
          "override": true,
          "name": "huypuy",
          "analyzer": "test_text",
          "features": ["norm", "frequency"],
          "isArray":false
        },
        {
          "name": "foo",
          "analyzer": "test_text",
          "features": ["norm", "frequency", "position"],
          "includeAllFields":true
        },
        {
          "expression": "RETURN SPLIT(@param, ',') ",
          "override": true,
          "name": "huypuy2",
          "analyzer": "test_text",
          "features": ["norm", "frequency"],
          "isArray":true,
          "trackListPositions": true
        },
        {
          "name": "foo.boo.too[*].doo.aoo.noo",
          "features": ["norm"]
        },
        {
          "name": "foo.boo.nest",
          "features": ["norm"],
          "analyzer": "test_text",
          "nested": [
            { "name":"A" },
            {
              "name":"Sub", "analyzer":"identity",
              "nested": [
                 { "name":"SubSub.foo",
                   "analyzer":"test_text",
                   "features": ["position"]}]
            }
          ]
        }
    ],
    "consistency": "immediate",
    "primarySort": [{ "field" : "dummy", "direction": "desc" }],
    "version":0,
    "storedValues": [{ "fields": ["dummy"], "compression": "none"}],
    "analyzer": "test_text",
    "features": ["norm", "position", "frequency"],
    "includeAllFields":true,
    "trackListPositions": false,
    "analyzerDefinitions":[{"name":"test_text", "type":"identity", "properties":{}}]
  })");

  /*
      "primarySort": {
       "fields":[{ "field" : "dummy", "direction": "desc" }],
       "compression": "none",
       "locale": "myLocale"
    },
  */
  arangodb::iresearch::IResearchInvertedIndexMeta meta;
  std::string errorString;
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  auto res = meta.init(server.server(), json->slice(), true, errorString,
                       irs::string_ref(vocbase.name()));
  {
    SCOPED_TRACE(::testing::Message("Unexpected error:") << errorString);
    ASSERT_TRUE(res);
  }
  ASSERT_TRUE(errorString.empty());
  ASSERT_EQ(2, meta._analyzerDefinitions.size());
  ASSERT_NE(meta._analyzerDefinitions.find(vocbase.name() + "::test_text"),
            meta._analyzerDefinitions.end());
  ASSERT_NE(meta._analyzerDefinitions.find("identity"),
            meta._analyzerDefinitions.end());
  ASSERT_EQ(6, meta._fields.size());
  ASSERT_FALSE(meta._sort.empty());
  ASSERT_FALSE(meta._storedValues.empty());
  ASSERT_EQ(meta._sortCompression, irs::type<irs::compression::lz4>::id());
  ASSERT_TRUE(meta.dense());
  ASSERT_EQ(meta._version,
            static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MIN));
  ASSERT_EQ(meta._consistency, Consistency::kImmediate);
  ASSERT_TRUE(meta._defaultAnalyzerName.empty());
  ASSERT_FALSE(meta._features);

  VPackBuilder serialized;
  ASSERT_TRUE(meta.json(server.server(), serialized, true, &vocbase));


}
