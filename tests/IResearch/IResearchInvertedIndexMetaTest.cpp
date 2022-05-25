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
/// @author Alexey Bakharew
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
  void analyzerFeaturesChecker(Features const& features, std::vector<std::string> expected) {
    VPackBuilder b;
    features.toVelocyPack(b);
    auto featuresSlice = b.slice();

    ASSERT_TRUE(featuresSlice.isArray());
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

  void serializationChecker(ArangodServer& server, std::string_view jsonAsSring) {
    auto json = VPackParser::fromJson({jsonAsSring.data(), jsonAsSring.size()});

    arangodb::iresearch::IResearchInvertedIndexMeta metaLhs, metaRhs;
    std::string errorString;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server));
    auto res = metaLhs.init(server, json->slice(), true, errorString,
                         irs::string_ref(vocbase.name()));
    {
      SCOPED_TRACE(::testing::Message("Unexpected error:") << errorString);
      ASSERT_TRUE(res);
    }
    ASSERT_TRUE(errorString.empty());


    VPackBuilder serializedLhs;
    {
      VPackObjectBuilder obj(&serializedLhs);
      ASSERT_TRUE(metaLhs.json(server, serializedLhs, true, &vocbase));
    }
    //  std::cout << serializedLhs.slice().toString() << std::endl << std::endl<< std::endl << std::endl;

    res = metaRhs.init(server, serializedLhs.slice(), true, errorString,
                         irs::string_ref(vocbase.name()));
    {
      SCOPED_TRACE(::testing::Message("Unexpected error:") << errorString);
      ASSERT_TRUE(res);
    }
    ASSERT_TRUE(errorString.empty());

    VPackBuilder serializedRhs;
    {
      VPackObjectBuilder obj(&serializedRhs);
      ASSERT_TRUE(metaLhs.json(server, serializedRhs, true, &vocbase));

    }
    //  std::cout << serializedRhs.slice().toString() << std::endl;

    ASSERT_EQ(serializedLhs.slice().toString(), serializedRhs.slice().toString());
    //  ASSERT_EQ(metaLhs, metaRhs); // FIXME: PrimarySort, StoredValues and etc should present in metaRhs. At this momemnt we loose it since serialization works wrong

  }
}

const std::string complexJsonDefinition1 = R"(
{
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
          {
            "name":"A"
          },
          {
            "name":"Sub",
            "analyzer":"identity",
            "nested": [
               {
                 "expression": "RETURN SPLIT(@param, '.') ",
                 "override": true,
                 "name":"SubSub.foo",
                 "analyzer":"test_text",
                 "features": ["position"]
               },
               {
                  "name": "woo",
                  "features": ["norm", "frequency"],
                  "override": false,
                  "includeAllFields":true,
                  "trackListPositions": true
               }
            ]
          }
        ]
      },
      {
        "name": "foobar.baz[*].bam",
        "features": ["norm"],
        "nested": [
           {
             "expression": "RETURN SPLIT(@param, '#') ",
             "name":"bus.duz",
             "override": true,
             "features": ["position"]
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
})";

const std::string complexJsonDefinition2 = R"(
{
  "fields": [
     "dummy",
    {
      "name": "foo",
      "analyzer": "identity",
      "expression": "Abc"
    },
    {
      "name": "foo.boo",
      "analyzer": "delimiter_analyzer"
    },
    {
      "name": "foo.goo",
      "analyzer": "stem_analyzer"
    }
  ],
  "primarySort": {
     "fields":[
        {
           "field" : "foo",
           "direction": "asc"
        },
        {
           "field" : "foo.boo",
           "direction": "desc"
        }
     ],
     "compression": "lz4",
     "locale": "de_DE@phonebook"
  },
  "consistency": "eventual",
  "version":1,
  "storedValues": [
    {
      "fields": ["foo.boo"],
      "compression": "lz4"
    },
    {
      "fields": ["foo.goo"],
      "compression": "lz4"
    }
  ],
  "includeAllFields":false,
  "trackListPositions": true,
  "analyzerDefinitions":[
    {
     "name":"delimiter_analyzer",
     "type":"delimiter",
     "properties": {
       "delimiter" : "."
     },
     "features": ["frequency"]
    },
    {
     "name":"stem_analyzer",
     "type":"stem",
     "properties": {
       "locale": "en.utf-8"
     },
     "features": ["norm"]
    }
 ]
 })";
// "analyzer": "delimiter_analyzer", FIXME

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

TEST_F(IResearchInvertedIndexMetaTest, test_wrongDefinition) {

  // Nested is incompatibe with trackListPositions
  // invalid analyzer
  // not existing analyzer
  // analyzer with only 'position' feature
  // invalid features
  // invalid feature name
  // invalid field names
  // define field name more than 1 time
  // only one expansion [*] is allowed
  // expansion [*] in nested is not allowed


}

TEST_F(IResearchInvertedIndexMetaTest, test_normalization) {

//  {
//    "fields": [
//       "simple"
//    ]
//  }


}

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

TEST_F(IResearchInvertedIndexMetaTest, test_readCustomizedValues2) {

  auto json = VPackParser::fromJson(complexJsonDefinition2);

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

  ASSERT_EQ(3, meta._analyzerDefinitions.size());
  ASSERT_NE(meta._analyzerDefinitions.find(vocbase.name() + "::delimiter_analyzer"),
            meta._analyzerDefinitions.end());
  ASSERT_NE(meta._analyzerDefinitions.find(vocbase.name() + "::stem_analyzer"),
            meta._analyzerDefinitions.end());
  ASSERT_NE(meta._analyzerDefinitions.find("identity"),
            meta._analyzerDefinitions.end());

  // Check primary sort
  ASSERT_FALSE(meta._sort.empty());
  const auto& primarySortFields = meta._sort.fields();
  ASSERT_EQ(primarySortFields.size(), 2);
  {
    auto& psField0 = primarySortFields[0];
    ASSERT_EQ(psField0.size(), 1);
    ASSERT_EQ(psField0[0].name, "foo");
    ASSERT_FALSE(psField0[0].shouldExpand);
  }
  {
    auto& psField1 = primarySortFields[1];
    ASSERT_EQ(psField1.size(), 2);
    ASSERT_EQ(psField1[0].name, "foo");
    ASSERT_FALSE(psField1[0].shouldExpand);

    ASSERT_EQ(psField1[1].name, "boo");
    ASSERT_FALSE(psField1[1].shouldExpand);
  }

  ASSERT_TRUE(meta._sort.direction(0));
  ASSERT_FALSE(meta._sort.direction(1));
  ASSERT_EQ(meta._sort.Locale(), "de_DE_PHONEBOOK");
  EXPECT_EQ(irs::type<irs::compression::lz4>::id(),
            meta._sort.sortCompression());

  ASSERT_TRUE(meta.dense());

  // Check stored values
  ASSERT_FALSE(meta._storedValues.empty());
  auto storedValues = meta._storedValues.columns();
  ASSERT_EQ(storedValues.size(), 2);
  {
    auto& storedValuesField0 = storedValues[0];
    ASSERT_FALSE(storedValuesField0.name.empty()); // field name with delimiter. Hard to compare it. Just check emptyness
    ASSERT_EQ(storedValuesField0.compression().id(), irs::type<irs::compression::lz4>::id());
    ASSERT_EQ(storedValuesField0.fields.size(), 1);
    ASSERT_EQ(storedValuesField0.fields[0].first, "foo.boo"); // Now verify field name
  }
  {
    auto& storedValuesField1 = storedValues[1];
    ASSERT_FALSE(storedValuesField1.name.empty()); // field name with delimiter. Hard to compare it. Just check emptyness
    ASSERT_EQ(storedValuesField1.compression().id(), irs::type<irs::compression::lz4>::id());
    ASSERT_EQ(storedValuesField1.fields.size(), 1);
    ASSERT_EQ(storedValuesField1.fields[0].first, "foo.goo"); // Now verify field name
  }

  ASSERT_EQ(meta._version,
            static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MAX));
  ASSERT_EQ(meta._consistency, Consistency::kEventual);
  ASSERT_TRUE(meta._defaultAnalyzerName.empty()); // FIXME: HOW COME?
  ASSERT_FALSE(meta._features);
  ASSERT_FALSE(meta._includeAllFields);
  ASSERT_TRUE(meta._trackListPositions);

  ASSERT_EQ(4, meta._fields.size());

  // Iterating through fields and check them
  {
    auto& field0 = meta._fields[0];

    ASSERT_EQ(field0.attribute().size(), 1);
    auto& attr = field0.attribute()[0];
    ASSERT_EQ(attr.name, "dummy");
    ASSERT_FALSE(attr.shouldExpand);

    ASSERT_FALSE(field0.overrideValue());
    ASSERT_EQ(field0.expansion().size(), 0);
    ASSERT_EQ(field0.nested().size(), 0);
    ASSERT_EQ(field0.expression().size(), 0);

    ASSERT_EQ(field0.analyzerName(), "identity"); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_FALSE(field0.features().has_value()); // Features are not specified in current field
    ASSERT_FALSE(meta._features.has_value()); // However, we will use features from meta
    //analyzerFeaturesChecker(meta._features.value(), {"norm", "frequency", "position"});

    ASSERT_FALSE(field0.isArray());
    ASSERT_FALSE(field0.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field0.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field1 = meta._fields[1];

    ASSERT_EQ(field1.attribute().size(), 1);
    auto& attr = field1.attribute()[0];
    ASSERT_EQ(attr.name, "foo");
    ASSERT_FALSE(attr.shouldExpand);

    ASSERT_FALSE(field1.overrideValue());
    ASSERT_EQ(field1.expansion().size(), 0);
    ASSERT_EQ(field1.nested().size(), 0);
    ASSERT_EQ(field1.expression(), "Abc");

    ASSERT_EQ(field1.analyzerName(), "identity"); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_FALSE(field1.features().has_value()); // Features are not specified in current field
    ASSERT_FALSE(meta._features.has_value()); // However, we will use features from meta
    analyzerFeaturesChecker(field1.analyzer()->features(), {"norm", "frequency", "position"});

    ASSERT_FALSE(field1.isArray());
    ASSERT_FALSE(field1.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field1.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field2 = meta._fields[2];

    ASSERT_EQ(field2.attribute().size(), 2);
    {
      auto& attr0 = field2.attribute()[0];
      ASSERT_EQ(attr0.name, "foo");
      ASSERT_FALSE(attr0.shouldExpand);
    }
    {
      auto& attr1 = field2.attribute()[1];
      ASSERT_EQ(attr1.name, "boo");
      ASSERT_FALSE(attr1.shouldExpand);
    }

    ASSERT_FALSE(field2.overrideValue());
    ASSERT_EQ(field2.expansion().size(), 0);
    ASSERT_EQ(field2.nested().size(), 0);
    ASSERT_EQ(field2.expression(), "");

    ASSERT_EQ(field2.analyzerName(), vocbase.name() + "::delimiter_analyzer"); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_FALSE(field2.features().has_value()); // Features are not specified in current field
    ASSERT_FALSE(meta._features.has_value()); // However, we will use features from meta
    analyzerFeaturesChecker(field2.analyzer()->features(), {"frequency"});

    ASSERT_FALSE(field2.isArray());
    ASSERT_FALSE(field2.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field2.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field3 = meta._fields[3];

    ASSERT_EQ(field3.attribute().size(), 2);
    {
      auto& attr0 = field3.attribute()[0];
      ASSERT_EQ(attr0.name, "foo");
      ASSERT_FALSE(attr0.shouldExpand);
    }
    {
      auto& attr1 = field3.attribute()[1];
      ASSERT_EQ(attr1.name, "goo");
      ASSERT_FALSE(attr1.shouldExpand);
    }

    ASSERT_FALSE(field3.overrideValue());
    ASSERT_EQ(field3.expansion().size(), 0);
    ASSERT_EQ(field3.nested().size(), 0);
    ASSERT_EQ(field3.expression(), "");

    ASSERT_EQ(field3.analyzerName(), vocbase.name() + "::stem_analyzer"); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_FALSE(field3.features().has_value()); // Features are not specified in current field
    ASSERT_FALSE(meta._features.has_value()); // However, we will use features from meta
    analyzerFeaturesChecker(meta._features.value(), {"norm"});

    ASSERT_FALSE(field3.isArray());
    ASSERT_FALSE(field3.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field3.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }





}


TEST_F(IResearchInvertedIndexMetaTest, test_readCustomizedValues1) {
  // without active vocbase
  auto json = VPackParser::fromJson(complexJsonDefinition1);

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

  ASSERT_EQ(7, meta._fields.size());

  // Check primary sort
  ASSERT_FALSE(meta._sort.empty());
  const auto& primarySortFields = meta._sort.fields();
  ASSERT_EQ(primarySortFields.size(), 1);
  auto& psField = primarySortFields[0];
  ASSERT_EQ(psField.size(), 1);
  ASSERT_EQ(psField[0].name, "foo");
  ASSERT_FALSE(psField[0].shouldExpand);
  ASSERT_FALSE(meta._sort.direction(0));
  ASSERT_EQ(meta._sort.Locale(), "mylocale");
  EXPECT_EQ(irs::type<irs::compression::lz4>::id(),
            meta._sort.sortCompression());

  ASSERT_TRUE(meta.dense());

  // Check stored values
  ASSERT_FALSE(meta._storedValues.empty());
  auto storedValues = meta._storedValues.columns();
  ASSERT_EQ(storedValues.size(), 1);
  ASSERT_FALSE(storedValues[0].name.empty()); // field name with delimiter. Hard to compare it. Just check emptyness
  ASSERT_EQ(storedValues[0].compression().id(), irs::type<irs::compression::none>::id());
  ASSERT_EQ(storedValues[0].fields.size(), 1);
  ASSERT_EQ(storedValues[0].fields[0].first, "foo.boo.nest"); // Now verify field name

  ASSERT_EQ(meta._version,
            static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MIN));
  ASSERT_EQ(meta._consistency, Consistency::kImmediate);
  ASSERT_TRUE(meta._defaultAnalyzerName.empty()); // FIXME: HOW COME?
  ASSERT_TRUE(meta._features);
  ASSERT_TRUE(meta._includeAllFields);
  ASSERT_FALSE(meta._trackListPositions);

  VPackBuilder serialized;
  {
    VPackObjectBuilder obj(&serialized);
    ASSERT_TRUE(meta.json(server.server(), serialized, false, &vocbase));

  }
//  std::cout << serialized.slice().toString() << std::endl;

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
    ASSERT_FALSE(field0.features().has_value()); // Features are not specified in current field
    ASSERT_TRUE(meta._features.has_value()); // However, we will use features from meta
    analyzerFeaturesChecker(meta._features.value(), {"norm", "frequency", "position"});

    ASSERT_FALSE(field0.isArray());
    ASSERT_FALSE(field0.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field0.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
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
    analyzerFeaturesChecker(field1.features().value(), {"norm", "frequency"});


    ASSERT_FALSE(field1.isArray());
    ASSERT_FALSE(field1.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field1.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
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
    analyzerFeaturesChecker(field2.features().value(), {"position", "norm", "frequency"});

    ASSERT_FALSE(field2.isArray());
    ASSERT_FALSE(field2.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_TRUE(field2.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
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
    analyzerFeaturesChecker(field3.features().value(), {"frequency", "norm"});

    ASSERT_TRUE(field3.isArray());
    ASSERT_TRUE(field3.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field3.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field4 = meta._fields[4];

    ASSERT_EQ(field4.attribute().size(), 3);
    {
      auto& attr1 = field4.attribute()[0];
      ASSERT_EQ(attr1.name, "foo");
      ASSERT_FALSE(attr1.shouldExpand);
    }
    {
      auto& attr2 = field4.attribute()[1];
      ASSERT_EQ(attr2.name, "boo");
      ASSERT_FALSE(attr2.shouldExpand);
    }
    {
      auto& attr3 = field4.attribute()[2];
      ASSERT_EQ(attr3.name, "too");
      ASSERT_TRUE(attr3.shouldExpand);
    }
    {
      auto& expansions = field4.expansion();
      ASSERT_EQ(field4.expansion().size(), 3);
      {
        auto& attr4 = expansions[0];
        ASSERT_EQ(attr4.name, "doo");
        ASSERT_FALSE(attr4.shouldExpand);
      }
      {
        auto& attr5 = expansions[1];
        ASSERT_EQ(attr5.name, "aoo");
        ASSERT_FALSE(attr5.shouldExpand);
      }
      {
        auto& attr6 = expansions[2];
        ASSERT_EQ(attr6.name, "noo");
        ASSERT_FALSE(attr6.shouldExpand);
      }
    }

    ASSERT_FALSE(field4.overrideValue());
    ASSERT_EQ(field4.expansion().size(), 3);
    ASSERT_EQ(field4.nested().size(), 0);
    ASSERT_EQ(field4.expression(), "");

    ASSERT_EQ(field4.analyzerName(), "identity"); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_TRUE(field4.features().has_value());
    analyzerFeaturesChecker(field4.features().value(), {"norm"});

    ASSERT_TRUE(field4.isArray()); // IS IT ACTUALLY TRUE?
    ASSERT_FALSE(field4.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field4.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field5 = meta._fields[5];

    ASSERT_EQ(field5.attribute().size(), 3);
    {
      auto& attr1 = field5.attribute()[0];
      ASSERT_EQ(attr1.name, "foo");
      ASSERT_FALSE(attr1.shouldExpand);
    }
    {
      auto& attr2 = field5.attribute()[1];
      ASSERT_EQ(attr2.name, "boo");
      ASSERT_FALSE(attr2.shouldExpand);
    }
    {
      auto& attr3 = field5.attribute()[2];
      ASSERT_EQ(attr3.name, "nest");
      ASSERT_FALSE(attr3.shouldExpand);
    }

    ASSERT_FALSE(field5.overrideValue());
    ASSERT_EQ(field5.expansion().size(), 0);
    ASSERT_EQ(field5.expression(), "");

    ASSERT_EQ(field5.analyzerName(), vocbase.name() + "::test_text"); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_TRUE(field5.features().has_value());
    analyzerFeaturesChecker(field5.features().value(), {"norm"});

    ASSERT_FALSE(field5.isArray());
    ASSERT_FALSE(field5.trackListPositions());
    ASSERT_FALSE(field5.includeAllFields());

    ASSERT_EQ(field5.nested().size(), 2);
    {
      auto& nested0 = field5.nested()[0];

      ASSERT_EQ(nested0.attribute().size(), 1);
      auto& attr = nested0.attribute()[0];
      ASSERT_EQ(attr.name, "A");
      ASSERT_FALSE(attr.shouldExpand);

      ASSERT_FALSE(nested0.overrideValue());
      ASSERT_EQ(nested0.expansion().size(), 0);
      ASSERT_EQ(nested0.nested().size(), 0);
      ASSERT_EQ(nested0.expression(), "");

      ASSERT_EQ(nested0.analyzerName(), "identity"); // FIXME: SHOULD BE DEFAULT VALUE FROM PARENT FIELD
      ASSERT_FALSE(nested0.features().has_value()); // Features are not specified in current field
      ASSERT_TRUE(meta._features.has_value()); // However, we will use features from meta
      analyzerFeaturesChecker(meta._features.value(), {"norm", "position", "frequency"});

      ASSERT_FALSE(nested0.isArray());
      ASSERT_FALSE(nested0.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      ASSERT_FALSE(nested0.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    }
    {
      auto& nested1 = field5.nested()[1];

      ASSERT_EQ(nested1.attribute().size(), 1);
      auto& attr = nested1.attribute()[0];
      ASSERT_EQ(attr.name, "Sub");
      ASSERT_FALSE(attr.shouldExpand);

      ASSERT_FALSE(nested1.overrideValue());
      ASSERT_EQ(nested1.expansion().size(), 0);
      ASSERT_EQ(nested1.expression(), "");

      ASSERT_EQ(nested1.analyzerName(), "identity");
      ASSERT_FALSE(nested1.features().has_value()); // Features are not specified in current field
      ASSERT_TRUE(meta._features.has_value()); // However, we will use features from meta
      analyzerFeaturesChecker(meta._features.value(), {"frequency", "position", "norm"});

      ASSERT_FALSE(nested1.isArray());
      ASSERT_FALSE(nested1.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      ASSERT_FALSE(nested1.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT

      ASSERT_EQ(nested1.nested().size(), 2);

      {
        auto& nested10 = nested1.nested()[0];

        ASSERT_EQ(nested10.attribute().size(), 2);
        {
          auto& nested10Attr0 = nested10.attribute()[0];
          ASSERT_EQ(nested10Attr0.name, "SubSub");
          ASSERT_FALSE(nested10Attr0.shouldExpand);
        }
        {
          auto& nested10Attr1 = nested10.attribute()[1];
          ASSERT_EQ(nested10Attr1.name, "foo");
          ASSERT_FALSE(nested10Attr1.shouldExpand);
        }

        ASSERT_TRUE(nested10.overrideValue());
        ASSERT_EQ(nested10.expansion().size(), 0);
        ASSERT_EQ(nested10.expression(), "RETURN SPLIT(@param, '.') ");

        ASSERT_EQ(nested10.analyzerName(), vocbase.name() + "::test_text");
        ASSERT_TRUE(nested10.features().has_value());
        analyzerFeaturesChecker(nested10.features().value(), {"position"});

        ASSERT_FALSE(nested10.isArray());
        ASSERT_FALSE(nested10.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
        ASSERT_FALSE(nested10.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      }

      {
        auto& nested11 = nested1.nested()[1];

        ASSERT_EQ(nested11.attribute().size(), 1);
        {
          auto& nested11Attr0 = nested11.attribute()[0];
          ASSERT_EQ(nested11Attr0.name, "woo");
          ASSERT_FALSE(nested11Attr0.shouldExpand);
        }

        ASSERT_FALSE(nested11.overrideValue());
        ASSERT_EQ(nested11.expansion().size(), 0);
        ASSERT_EQ(nested11.expression(), "");

        ASSERT_EQ(nested11.analyzerName(), "identity"); // FIXME: SHOULD BE DEFAULT VALUE FROM PARENT FIELD
        ASSERT_TRUE(nested11.features().has_value());
        analyzerFeaturesChecker(nested11.features().value(), {"norm", "frequency"});

        ASSERT_FALSE(nested11.isArray());
        ASSERT_TRUE(nested11.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
        ASSERT_TRUE(nested11.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      }
    }
  }

  {
    auto& field6 = meta._fields[6];

    ASSERT_EQ(field6.attribute().size(), 2);
    {
      auto& attr0 = field6.attribute()[0];
      ASSERT_EQ(attr0.name, "foobar");
      ASSERT_FALSE(attr0.shouldExpand);
    }
    {
      auto& attr1 = field6.attribute()[1];
      ASSERT_EQ(attr1.name, "baz");
      ASSERT_TRUE(attr1.shouldExpand);
    }
    ASSERT_EQ(field6.expansion().size(), 1);
    {
      auto& attr2 = field6.expansion()[0];
      ASSERT_EQ(attr2.name, "bam");
      ASSERT_FALSE(attr2.shouldExpand);

    }

    ASSERT_EQ(field6.expansion().size(), 1);
    ASSERT_FALSE(field6.overrideValue());
    ASSERT_EQ(field6.expression(), "");

    ASSERT_EQ(field6.analyzerName(), "identity"); // FIXME: SHOULD BE DEFAULT VALUE FROM PARENT FIELD
    ASSERT_TRUE(field6.features().has_value());
    analyzerFeaturesChecker(field6.features().value(), {"norm"});

    ASSERT_TRUE(field6.isArray());
    ASSERT_FALSE(field6.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field6.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT

    ASSERT_EQ(field6.nested().size(), 1);
    {
      auto& nested = field6.nested()[0];

      ASSERT_EQ(nested.attribute().size(), 2);
      {
        auto& attr0 = nested.attribute()[0];
        ASSERT_EQ(attr0.name, "bus");
        ASSERT_FALSE(attr0.shouldExpand);
      }
      {
        auto& attr1 = nested.attribute()[1];
        ASSERT_EQ(attr1.name, "duz");
        ASSERT_FALSE(attr1.shouldExpand);
      }

      ASSERT_EQ(nested.expression(), "RETURN SPLIT(@param, '#') ");
      analyzerFeaturesChecker(nested.features().value(), {"position"});
      ASSERT_TRUE(nested.overrideValue());

      ASSERT_FALSE(nested.isArray());
      ASSERT_FALSE(nested.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      ASSERT_FALSE(nested.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    }
  }
}

TEST_F(IResearchInvertedIndexMetaTest, test_serialization) {

  std::vector<std::string_view> jsons{complexJsonDefinition1, complexJsonDefinition2};

  for (const auto& json : jsons) {
    serializationChecker(server.server(), json);
  }
}
