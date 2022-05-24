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
#include "IResearch/IResearchInvertedIndexMeta.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "VocBase/Methods/Collections.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"

using namespace arangodb;
using namespace arangodb::iresearch;

namespace  {
  void AnalyzerFeaturesChecker(Features const& features, std::vector<std::string> expected) {
    VPackBuilder b;
    features.toVelocyPack(b);
    auto featuresSlice = b.slice();

    featuresSlice.isArray();
    std::vector<std::string> actual;
    for (auto itr : VPackArrayIterator(featuresSlice)) {
      actual.push_back(itr.copyString());
    }

    ASSERT_EQ(actual.size(), expected.size());
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());

    auto it1 = expected.begin();
    auto it2 = actual.begin();
    while (it1 != expected.end() && it2 != actual.end()) {
      ASSERT_EQ(*it1, *it2);
      it1++;
      it2++;
    }

}
}

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
  ASSERT_EQ(meta._sort.sortCompression(), irs::type<irs::compression::lz4>::id());
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
  // without active vocbase
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
    ASSERT_EQ(meta._sort.sortCompression(), irs::type<irs::compression::lz4>::id());
    ASSERT_TRUE(meta._analyzerDefinitions.empty());
    ASSERT_FALSE(meta.dense());
    ASSERT_EQ(meta._version,
              static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MAX));
    ASSERT_EQ(meta._consistency, Consistency::kEventual);
    ASSERT_TRUE(meta._defaultAnalyzerName.empty());
    ASSERT_FALSE(meta._features);
  }
  // with active vocbase
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
    ASSERT_EQ(meta._sort.sortCompression(), irs::type<irs::compression::lz4>::id());
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
  // without active vocbase
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
    "primarySort": {
       "fields":[{ "field" : "foo", "direction": "desc" }],
       "compression": "none",
       "locale": "myLocale"
    },
    "consistency": "immediate",
    "version":0,
    "storedValues": [{ "fields": ["foo.boo.nest"], "compression": "none"}],
    "analyzer": "test_text",
    "features": ["norm", "position", "frequency"],
    "includeAllFields":true,
    "trackListPositions": false,
    "analyzerDefinitions":[{"name":"test_text", "type":"identity", "properties":{}}]
  })");

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
  const auto& primarySortFields = meta._sort.fields();
  ASSERT_EQ(primarySortFields.size(), 1);
  auto& psField = primarySortFields[0];
  ASSERT_EQ(psField.size(), 1);
  ASSERT_EQ(psField[0].name, "foo");
  ASSERT_FALSE(psField[0].shouldExpand);
  ASSERT_FALSE(meta._sort.direction(0));
  ASSERT_EQ(meta._sort.Locale(), "mylocale");
//  meta._sort.empty()
//  meta._sort.sortCompression() // FIXME: HOW TO CHECK compression?
//  ASSERT_EQ(meta._sortCompression(), irs::type<irs::compression::lz4>::id()); FIXME: HOW TO CHECK compression

  // Check stored values
  ASSERT_FALSE(meta._storedValues.empty());
  ASSERT_EQ(meta._sort.sortCompression(), irs::type<irs::compression::lz4>::id());
  ASSERT_TRUE(meta.dense());
  auto storedValues = meta._storedValues.columns();
  ASSERT_EQ(storedValues.size(), 1);
  ASSERT_FALSE(storedValues[0].name.empty()); // field name with delimiter. Hard to compare it. Just check emptyness
//  ASSERT_EQ(storedValues[0].compression(), irs::compression::none()); FIXME: HOW TO CHECK compression
  ASSERT_EQ(storedValues[0].fields.size(), 1);
  ASSERT_EQ(storedValues[0].fields[0].first, "foo.boo.nest"); // Now verify field name


  ASSERT_TRUE(meta.dense()); // FIXME: WHAT IS IT?
  ASSERT_EQ(meta._version,
            static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MIN));
  ASSERT_EQ(meta._consistency, Consistency::kImmediate);
  ASSERT_TRUE(meta._defaultAnalyzerName.empty()); // FIXME: HOW COME?
  ASSERT_FALSE(meta._features); // FIXME: HOW COME?
  ASSERT_TRUE(meta._includeAllFields);
  ASSERT_FALSE(meta._trackListPositions);

  VPackBuilder serialized;
  {
    VPackObjectBuilder obj(&serialized);
    ASSERT_TRUE(meta.json(server.server(), serialized, true, &vocbase));
  }

  // Iterating through fields and check them
  {
    auto& field0 = meta._fields[0];

    ASSERT_EQ(field0.attribute().size(), 1);
    auto& attr = field0.attribute()[0];
    ASSERT_EQ(attr.name, "simple");
    ASSERT_FALSE(attr.shouldExpand);

    ASSERT_FALSE(field0.overrideValue());
    ASSERT_EQ(field0.expansion().size(), 0);
    ASSERT_EQ(field0.nested().size(), 0);
    ASSERT_EQ(field0.expression().size(), 0);

    ASSERT_EQ(field0.analyzerName(), "identity"); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_TRUE(meta._features.has_value()); // FIXME: FALSE. HOW COME??
    AnalyzerFeaturesChecker(meta._features.value(), {"norm", "frequency", "position"});//FIXME: FALSE. IS IDENTITY DOES NOT SUPPORT POSITION?

    ASSERT_FALSE(field0.isArray());
    ASSERT_FALSE(field0.trackListPositions());
    ASSERT_FALSE(field0.includeAllFields());
  }

  {
    auto& field1 = meta._fields[1];

    ASSERT_EQ(field1.attribute().size(), 1);
    auto& attr = field1.attribute()[0];
    ASSERT_EQ(attr.name, "huypuy");
    ASSERT_FALSE(attr.shouldExpand);

    ASSERT_TRUE(field1.overrideValue());
    ASSERT_EQ(field1.expansion().size(), 0);
    ASSERT_EQ(field1.nested().size(), 0);
    ASSERT_EQ(field1.expression(), "RETURN MERGE(@param, {foo: 'bar'}) ");

    ASSERT_EQ(field1.analyzerName(), vocbase.name() + "::test_text");
    ASSERT_TRUE(field1.features().has_value());
    AnalyzerFeaturesChecker(field1.features().value(), {"norm", "frequency"});


    ASSERT_FALSE(field1.isArray());
    ASSERT_FALSE(field1.trackListPositions());
    ASSERT_FALSE(field1.includeAllFields());
  }

  {
    auto& field2 = meta._fields[2];
    ASSERT_EQ(field2.attribute().size(), 1);
    auto& attr = field2.attribute()[0];
    ASSERT_EQ(attr.name, "foo");
    ASSERT_FALSE(attr.shouldExpand);

    ASSERT_FALSE(field2.overrideValue());
    ASSERT_EQ(field2.expansion().size(), 0);
    ASSERT_EQ(field2.nested().size(), 0);
    ASSERT_EQ(field2.expression(), "");

    ASSERT_EQ(field2.analyzerName(), vocbase.name() + "::test_text");
    ASSERT_TRUE(field2.features().has_value());
    AnalyzerFeaturesChecker(field2.features().value(), {"position", "norm", "frequency"});

    ASSERT_FALSE(field2.isArray());
    ASSERT_FALSE(field2.trackListPositions());
    ASSERT_TRUE(field2.includeAllFields());
  }

  {
    auto& field3 = meta._fields[3];

    ASSERT_EQ(field3.attribute().size(), 1);
    auto& attr = field3.attribute()[0];
    ASSERT_EQ(attr.name, "huypuy2");
    ASSERT_FALSE(attr.shouldExpand);

    ASSERT_TRUE(field3.overrideValue());
    ASSERT_EQ(field3.expansion().size(), 0);
    ASSERT_EQ(field3.nested().size(), 0);
    ASSERT_EQ(field3.expression(), "RETURN SPLIT(@param, ',') ");

    ASSERT_EQ(field3.analyzerName(), vocbase.name() + "::test_text");
    ASSERT_TRUE(field3.features().has_value());
    AnalyzerFeaturesChecker(field3.features().value(), {"frequency", "norm"});

    ASSERT_TRUE(field3.isArray());
    ASSERT_TRUE(field3.trackListPositions());
    ASSERT_FALSE(field3.includeAllFields());
  }

  {
    auto& field4 = meta._fields[4];
  }

  {
    auto& field5 = meta._fields[5];
  }









//  VPackBuilder serialized;
//  ASSERT_TRUE(meta.json(server.server(), serialized, true, &vocbase));

}

