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
using namespace arangodb::basics;
using namespace std::literals;

namespace {
  void analyzerFeaturesChecker(std::vector<std::string> expected, Features const& features) {
    VPackBuilder b;
    features.toVelocyPack(b);
    auto featuresSlice = b.slice();

    ASSERT_TRUE(featuresSlice.isArray());
    std::vector<std::string> actual;
    for (auto itr : VPackArrayIterator(featuresSlice)) {
      actual.push_back(itr.copyString());
    }

    ASSERT_EQ(expected.size(), actual.size());
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

  void serializationChecker(ArangodServer& server, std::string_view jsonAsString) {
    auto json = VPackParser::fromJson({jsonAsString.data(), jsonAsString.size()});

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

    ASSERT_EQ(serializedLhs.slice().toString(), serializedRhs.slice().toString());
    ASSERT_EQ(metaLhs, metaRhs); // FIXME: PrimarySort, StoredValues and etc should present in metaRhs. At this momemnt we loose it since serialization works wrong

  }
}

std::string_view complexJsonDefinition1 = R"(
{
  "fields": [
     "simple",
      {
        "expression": "RETURN MERGE(@param, {foo: 'bar'}) ",
        "override": true,
        "name": "field_name_1",
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
        "name": "field_name_2",
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

std::string_view complexJsonDefinition2 = R"(
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
      "analyzer": "delimiter_analyzer",
      "override": true
    },
    {
      "name": "foo.goo",
      "analyzer": "stem_analyzer",
      "override": true
    },
    {
      "name": "zoo[*]",
      "override": true,
      "nested": [
        "zoo",
        {
          "name": "doo",
          "analyzer": "stem_analyzer",
          "features": ["frequency"],
          "override": true,
          "includeAllFields":true,
          "trackListPositions": true
        }
      ]
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

TEST_F(IResearchInvertedIndexMetaTest, testWrongDefinition) {

  // Nested is incompatibe with trackListPositions
  std::string_view wrongDefinition1 = R"(
  {
    "fields": [
      {
        "name": "foo",
        "analyzer": "stem_analyzer",
        "nested": [
          {
            "name": "bar",
            "trackListPositions": true
          }
        ]
      }
    ],
    "trackListPositions": true,
    "analyzerDefinitions":[
      {
       "name":"stem_analyzer",
       "type":"stem",
       "properties": {
         "locale": "en.utf-8"
       },
       "features": ["norm"]
      }
   ]
   })"; // FIXME: This definition is not crashed


  // invalid analyzer
  std::string_view wrongDefinition2 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "identity"
          }
      ],
      "includeAllFields": true,
      "trackListPositions": true,
      "analyzerDefinitions": [
          {
              "name": "identity",
              "type": "identitu",
              "features": ["norm"]
          }
      ]
  })";

  // not existing analyzer
  std::string_view wrongDefinition3 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "wrong_analyzer",
              "nested": [
                  {
                      "name": "bar",
                      "trackListPositions": true
                  }
              ]
          }
      ],
      "analyzerDefinitions": [
          {
              "name": "stem_analyzer",
              "type": "stem",
              "properties": {
                  "locale": "en.utf-8"
              },
              "features": ["norm"]
          }
      ]
  })";

  // analyzer with only 'position' feature
  std::string_view wrongDefinition4 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "stem_analyzer",
              "nested": [
                  {
                      "name": "bar"
                  }
              ]
          }
      ],
      "analyzerDefinitions": [
          {
              "name": "stem_analyzer",
              "type": "stem",
              "properties": {
                  "locale": "en.utf-8"
              },
              "features": ["position"]
          }
      ]
  })";

  // invalid feature name
  std::string_view wrongDefinition5 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "stem_analyzer",
              "nested": [
                  {
                      "name": "bar"
                  }
              ]
          }
      ],
      "analyzerDefinitions": [
          {
              "name": "stem_analyzer",
              "type": "stem",
              "properties": {
                  "locale": "en.utf-8"
              },
              "features": ["features"]
          }
      ]
  })";

  // define field name more than 1 time
  std::string_view wrongDefinition6 = R"(
  {
      "fields": [
          {
              "name": "foo"
          },
          {
              "name": "foo"
          }
      ],
      "analyzerDefinitions": [
          {
            "name":"delimiter_analyzer",
            "type":"delimiter",
            "properties": {
              "delimiter" : "."
            },
            "features": ["frequency"]
          }
      ]
  })"; // FIXME: This definition is not crashed

  // only one expansion [*] is allowed
  std::string_view wrongDefinition7 = R"(
  {
      "fields": [
          {
              "name": "foo.bar.baz[*].bug.bus[*].bud",
              "analyzer": "stem_analyzer",
              "nested": [
                  {
                      "name": "bar"
                  }
              ]
          }
      ],
      "analyzerDefinitions": [
          {
              "name": "stem_analyzer",
              "type": "stem",
              "properties": {
                  "locale": "en.utf-8"
              },
              "features": ["features"]
          }
      ]
  })";

  // expansion [*] in nested is not allowed
  std::string_view wrongDefinition8 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "stem_analyzer",
              "nested": [
                  {
                      "name": "bar.bud[*].buz"
                  }
              ]
          }
      ],
      "analyzerDefinitions": [
          {
              "name": "stem_analyzer",
              "type": "stem",
              "properties": {
                  "locale": "en.utf-8"
              },
              "features": ["features"]
          }
      ]
  })";

  // "fields" in "primarySort" is empty
  std::string_view wrongDefinition9 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "identity",
              "nested": [
                  {
                      "name": "bar.bud[*].buz"
                  }
              ]
          }
      ],
      "primarySort": {
         "fields":[],
         "compression": "none",
         "locale": "myLocale"
      }
  })";

  // Empty fields array
  std::string_view wrongDefinition10 = R"(
  {
    "fields": [],
    "trackListPositions": true,
    "analyzerDefinitions":[
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

  // empty object
  std::string_view wrongDefinition11 = R"({})";




  std::array<std::string_view, 9> badJsons{
//    wrongDefinition1, //FIXME: This definition is not failing
    wrongDefinition2,
    wrongDefinition3,
    wrongDefinition4,
    wrongDefinition5,
//    wrongDefinition6, //FIXME: This definition is not failing
    wrongDefinition7,
    wrongDefinition8,
    wrongDefinition9,
    wrongDefinition10,
    wrongDefinition11
  };

  int i = 1;
  for (auto jsonD : badJsons) {
    auto json = VPackParser::fromJson(jsonD.data(), jsonD.size());

    arangodb::iresearch::IResearchInvertedIndexMeta meta;
    std::string errorString;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto res = meta.init(server.server(), json->slice(), true, errorString,
                         irs::string_ref(vocbase.name()));
    {
      SCOPED_TRACE(::testing::Message("Unexpected error in ") << i << " definition: " << errorString);
      ASSERT_FALSE(res);
    }
    ASSERT_FALSE(errorString.empty());
    ++i;
  }
}

TEST_F(IResearchInvertedIndexMetaTest, testCornerCases) {

  // Empty analyzerDefinitions array
  std::string_view definition2 = R"(
  {
    "fields": ["foo"],
    "analyzerDefinitions":[]
   })";


  // "fields" in storedValues is empty
  std::string_view definition4 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "identity"
          }
      ],
      "storedValues": [
          {
              "fields": [],
              "compression": "none"
          }
      ]
  })";

  // "fields" in storedValues is empty
  std::string_view definition5 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "identity"
          }
      ],
      "storedValues": [{ "fields": [], "compression": "none"}]
  })";

  // "nested" is empty
  std::string_view definition6 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "identity",
              "nested": []
          }
      ]
  })";

  // "features" is empty
  std::string_view definition7 = R"(
  {
    "fields": [
       "simple"
    ],
    "features": []
  })";

  // Duplication of analyzers names
  std::string_view definition8 = R"(
  {
    "fields": [
      "foo"
    ],
    "analyzerDefinitions":[
      {
        "name":"myAnalyzer",
        "type":"stem",
        "properties": {
          "locale": "en.utf-8"
        },
        "features": ["norm"]
      },
      {
        "name":"myAnalyzer",
        "type":"delimiter",
        "properties": {
          "delimiter" : "."
        },
        "features": ["frequency"]
      }
    ]
   })";


  std::array<std::string_view, 5> jsons{
//    definition2,
    definition4,
    definition5,
    definition6,
    definition7,
    definition8
  };

  int i = 1;
  for (auto jsonD : jsons) {
    auto json = VPackParser::fromJson(jsonD.data(), jsonD.size());

    arangodb::iresearch::IResearchInvertedIndexMeta meta;
    std::string errorString;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto res = meta.init(server.server(), json->slice(), true, errorString,
                         irs::string_ref(vocbase.name()));
    {
      SCOPED_TRACE(::testing::Message("Unexpected error in ") << i << " definition: " << errorString);
      ASSERT_TRUE(res);
    }
    ASSERT_TRUE(errorString.empty());
    ++i;
  }
}

TEST_F(IResearchInvertedIndexMetaTest, testDefaults) {
  arangodb::iresearch::IResearchInvertedIndexMeta meta;

  ASSERT_EQ(0, meta._analyzerDefinitions.size());
  ASSERT_TRUE(true == meta._fields.empty());
  ASSERT_TRUE(meta._sort.empty());
  ASSERT_TRUE(meta._storedValues.empty());
  ASSERT_EQ(irs::type<irs::compression::lz4>::id(), meta._sort.sortCompression());
  ASSERT_TRUE(meta._analyzerDefinitions.empty());
  ASSERT_FALSE(meta.dense());
  ASSERT_EQ(static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MAX), meta._version);
  ASSERT_EQ(Consistency::kEventual, meta. _consistency);
  ASSERT_TRUE(meta._defaultAnalyzerName.empty());
  ASSERT_FALSE(meta._features);
}

TEST_F(IResearchInvertedIndexMetaTest, testReadDefaults) {
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
    ASSERT_EQ(irs::type<irs::compression::lz4>::id(), meta._sort.sortCompression());
    ASSERT_TRUE(meta._analyzerDefinitions.empty());
    ASSERT_FALSE(meta.dense());
    ASSERT_EQ(static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MAX), meta._version);
    ASSERT_EQ(Consistency::kEventual, meta._consistency);
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
    ASSERT_EQ(irs::type<irs::compression::lz4>::id(), meta._sort.sortCompression());
    ASSERT_TRUE(meta._analyzerDefinitions.empty());
    ASSERT_FALSE(meta.dense());
    ASSERT_EQ(static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MAX), meta._version);
    ASSERT_EQ(Consistency::kEventual, meta._consistency);
    ASSERT_TRUE(meta._defaultAnalyzerName.empty());
    ASSERT_FALSE(meta._features);
  }
}

TEST_F(IResearchInvertedIndexMetaTest, testReadCustomizedValues1) {
  // without active vocbase

  std::string_view wrongDefinition10 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "identity"
          }
      ],
      "storedValues": [
          {
              "fields": ["foo"],
              "compression": "none"
          }
      ]
  })";

  auto json = VPackParser::fromJson(wrongDefinition10.data(), wrongDefinition10.size());

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
  ASSERT_EQ(1, primarySortFields.size());

  auto& psField = primarySortFields[0];
  ASSERT_EQ(1, psField.size());
  ASSERT_EQ(psField[0], (AttributeName{"foo"sv, false}));

  ASSERT_FALSE(meta._sort.direction(0));
  ASSERT_EQ("mylocale", meta._sort.Locale());
  EXPECT_EQ(irs::type<irs::compression::lz4>::id(),
            meta._sort.sortCompression());

  ASSERT_TRUE(meta.dense());

  // Check stored values
  ASSERT_FALSE(meta._storedValues.empty());
  auto storedValues = meta._storedValues.columns();
  ASSERT_EQ(1, storedValues.size());
  ASSERT_FALSE(storedValues[0].name.empty()); // field name with delimiter. Hard to compare it. Just check emptyness
  ASSERT_EQ(irs::type<irs::compression::none>::id(), storedValues[0].compression().id());
  ASSERT_EQ(1, storedValues[0].fields.size());
  ASSERT_EQ("foo.boo.nest", storedValues[0].fields[0].first); // Now verify field name

  ASSERT_EQ(meta._version,
            static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MIN));
  ASSERT_EQ(Consistency::kImmediate, meta._consistency);
  ASSERT_TRUE(meta._defaultAnalyzerName.empty()); // FIXME: HOW COME?
  ASSERT_TRUE(meta._features);
  ASSERT_TRUE(meta._includeAllFields);
  ASSERT_FALSE(meta._trackListPositions);

  VPackBuilder serialized;
  {
    VPackObjectBuilder obj(&serialized);
    ASSERT_TRUE(meta.json(server.server(), serialized, false, &vocbase));

  }

  // Iterating through fields and check them
  {
    auto& field0 = meta._fields[0];

    ASSERT_EQ(1, field0.attribute().size());
    ASSERT_EQ(field0.attribute()[0], (AttributeName{"simple"sv, false}));

    ASSERT_FALSE(field0.overrideValue());
    ASSERT_EQ(0, field0.expansion().size());
    ASSERT_EQ(0, field0.nested().size());
    ASSERT_EQ(0, field0.expression().size());

    ASSERT_EQ("identity", field0.analyzerName()); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_FALSE(field0.features().has_value()); // Features are not specified in current field
    ASSERT_TRUE(meta._features.has_value()); // However, we will use features from meta
    analyzerFeaturesChecker({"norm", "frequency", "position"}, meta._features.value());

    ASSERT_FALSE(field0.isArray());
    ASSERT_FALSE(field0.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_TRUE(field0.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field1 = meta._fields[1];

    ASSERT_EQ(1, field1.attribute().size());
    ASSERT_EQ(field1.attribute()[0], (AttributeName{"field_name_1"sv, false}));

    ASSERT_TRUE(field1.overrideValue());
    ASSERT_EQ(0, field1.expansion().size());
    ASSERT_EQ(0, field1.nested().size());
    ASSERT_EQ("RETURN MERGE(@param, {foo: 'bar'}) ", field1.expression());

    ASSERT_EQ(vocbase.name() + "::test_text", field1.analyzerName());
    ASSERT_TRUE(field1.features().has_value());
    analyzerFeaturesChecker({"norm", "frequency"}, field1.features().value());


    ASSERT_FALSE(field1.isArray());
    ASSERT_TRUE(field1.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field1.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field2 = meta._fields[2];
    ASSERT_EQ(1, field2.attribute().size());
    ASSERT_EQ(field2.attribute()[0], (AttributeName{"foo"sv, false}));

    ASSERT_FALSE(field2.overrideValue());
    ASSERT_EQ(0, field2.expansion().size());
    ASSERT_EQ(0, field2.nested().size());
    ASSERT_EQ("", field2.expression());

    ASSERT_EQ(vocbase.name() + "::test_text", field2.analyzerName());
    ASSERT_TRUE(field2.features().has_value());
    analyzerFeaturesChecker({"position", "norm", "frequency"}, field2.features().value());

    ASSERT_FALSE(field2.isArray());
    ASSERT_FALSE(field2.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_TRUE(field2.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field3 = meta._fields[3];

    ASSERT_EQ(1, field3.attribute().size());
    ASSERT_EQ(field3.attribute()[0], (AttributeName{"field_name_2"sv, false}));

    ASSERT_TRUE(field3.overrideValue());
    ASSERT_EQ(0, field3.expansion().size());
    ASSERT_EQ(0, field3.nested().size());
    ASSERT_EQ("RETURN SPLIT(@param, ',') ", field3.expression());

    ASSERT_EQ(vocbase.name() + "::test_text", field3.analyzerName());
    ASSERT_TRUE(field3.features().has_value());
    analyzerFeaturesChecker({"frequency", "norm"}, field3.features().value());

    ASSERT_TRUE(field3.isArray());
    ASSERT_TRUE(field3.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field3.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field4 = meta._fields[4];

    ASSERT_EQ(3, field4.attribute().size());

    std::vector<AttributeName> attrs{{"foo"sv, false}, {"boo"sv, false}, {"too"sv, true}};
    ASSERT_EQ(field4.attribute(), attrs);
    {
      // check expansion
      ASSERT_EQ(3, field4.expansion().size());
      std::vector<AttributeName> expAttrs{{"doo"sv, false}, {"aoo"sv, false}, {"noo"sv, false}};
      ASSERT_EQ(field4.expansion(), expAttrs);
    }

    ASSERT_FALSE(field4.overrideValue());
    ASSERT_EQ(3, field4.expansion().size());
    ASSERT_EQ(0, field4.nested().size());
    ASSERT_EQ("", field4.expression());

    ASSERT_EQ("identity", field4.analyzerName()); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_TRUE(field4.features().has_value());
    analyzerFeaturesChecker({"norm"}, field4.features().value());

    ASSERT_TRUE(field4.isArray()); // IS IT ACTUALLY TRUE?
    ASSERT_FALSE(field4.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_TRUE(field4.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field5 = meta._fields[5];

    ASSERT_EQ(3, field5.attribute().size());
    std::vector<AttributeName> attrs{{"foo"sv, false}, {"boo"sv, false}, {"nest"sv, false}};
    ASSERT_EQ(field5.attribute(), attrs);

    ASSERT_FALSE(field5.overrideValue());
    ASSERT_EQ(0, field5.expansion().size());
    ASSERT_EQ("", field5.expression());

    ASSERT_EQ(vocbase.name() + "::test_text", field5.analyzerName()); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_TRUE(field5.features().has_value());
    analyzerFeaturesChecker({"norm"}, field5.features().value());

    ASSERT_FALSE(field5.isArray());
    ASSERT_FALSE(field5.trackListPositions());
    ASSERT_TRUE(field5.includeAllFields());

    ASSERT_EQ(2, field5.nested().size());
    {
      auto& nested0 = field5.nested()[0];

      ASSERT_EQ(1, nested0.attribute().size());
      ASSERT_EQ(nested0.attribute()[0], (AttributeName{"A"sv, false}));

      ASSERT_FALSE(nested0.overrideValue());
      ASSERT_EQ(0, nested0.expansion().size());
      ASSERT_EQ(0, nested0.nested().size());
      ASSERT_EQ("", nested0.expression());

      ASSERT_EQ("identity", nested0.analyzerName()); // FIXME: SHOULD BE DEFAULT VALUE FROM PARENT FIELD
      ASSERT_FALSE(nested0.features().has_value()); // Features are not specified in current field
      ASSERT_TRUE(meta._features.has_value()); // However, we will use features from meta
      analyzerFeaturesChecker({"norm", "position", "frequency"}, meta._features.value());

      ASSERT_FALSE(nested0.isArray());
      ASSERT_FALSE(nested0.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      ASSERT_TRUE(nested0.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    }
    {
      auto& nested1 = field5.nested()[1];

      ASSERT_EQ(1, nested1.attribute().size());
      ASSERT_EQ(nested1.attribute()[0], (AttributeName{"Sub"sv, false}));

      ASSERT_FALSE(nested1.overrideValue());
      ASSERT_EQ(0, nested1.expansion().size());
      ASSERT_EQ("", nested1.expression());

      ASSERT_EQ("identity", nested1.analyzerName());
      ASSERT_FALSE(nested1.features().has_value()); // Features are not specified in current field
      ASSERT_TRUE(meta._features.has_value()); // However, we will use features from meta
      analyzerFeaturesChecker({"frequency", "position", "norm"}, meta._features.value());

      ASSERT_FALSE(nested1.isArray());
      ASSERT_FALSE(nested1.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      ASSERT_TRUE(nested1.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT

      ASSERT_EQ(2, nested1.nested().size());

      {
        auto& nested10 = nested1.nested()[0];

        ASSERT_EQ(2, nested10.attribute().size());

        ASSERT_EQ(nested1.attribute()[0], (AttributeName{"SubSub"sv, false}));
        ASSERT_EQ(nested1.attribute()[1], (AttributeName{"foo"sv, false}));

        ASSERT_TRUE(nested10.overrideValue());
        ASSERT_EQ(0, nested10.expansion().size());
        ASSERT_EQ("RETURN SPLIT(@param, '.') ", nested10.expression());

        ASSERT_EQ(vocbase.name() + "::test_text", nested10.analyzerName());
        ASSERT_TRUE(nested10.features().has_value());
        analyzerFeaturesChecker({"position"}, nested10.features().value());

        ASSERT_FALSE(nested10.isArray());
        ASSERT_FALSE(nested10.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
        ASSERT_TRUE(nested10.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      }

      {
        auto& nested11 = nested1.nested()[1];

        ASSERT_EQ(1, nested11.attribute().size());
        ASSERT_EQ(nested1.attribute()[0], (AttributeName{"woo"sv, false}));

        ASSERT_FALSE(nested11.overrideValue());
        ASSERT_EQ(0, nested11.expansion().size());
        ASSERT_EQ("", nested11.expression());

        ASSERT_EQ("identity", nested11.analyzerName()); // FIXME: SHOULD BE DEFAULT VALUE FROM PARENT FIELD
        ASSERT_TRUE(nested11.features().has_value());
        analyzerFeaturesChecker({"norm", "frequency"}, nested11.features().value());

        ASSERT_FALSE(nested11.isArray());
        ASSERT_TRUE(nested11.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
        ASSERT_TRUE(nested11.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      }
    }
  }

  {
    auto& field6 = meta._fields[6];

    ASSERT_EQ(2, field6.attribute().size());
    ASSERT_EQ(field6.attribute()[0], (AttributeName{"foobar"sv, false}));
    ASSERT_EQ(field6.attribute()[1], (AttributeName{"baz"sv, false}));

    ASSERT_EQ(1, field6.expansion().size());
    ASSERT_EQ(field6.expansion()[0], (AttributeName{"bam"sv, false}));

    ASSERT_EQ(1, field6.expansion().size());
    ASSERT_FALSE(field6.overrideValue());
    ASSERT_EQ("", field6.expression());

    ASSERT_EQ("identity", field6.analyzerName()); // FIXME: SHOULD BE DEFAULT VALUE FROM PARENT FIELD
    ASSERT_TRUE(field6.features().has_value());
    analyzerFeaturesChecker({"norm"}, field6.features().value());

    ASSERT_TRUE(field6.isArray());
    ASSERT_FALSE(field6.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_TRUE(field6.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT

    ASSERT_EQ(1, field6.nested().size());
    {
      auto& nested = field6.nested()[0];

      ASSERT_EQ(2, nested.attribute().size());
      ASSERT_EQ(nested.attribute()[0], (AttributeName{"bus"sv, false}));
      ASSERT_EQ(nested.attribute()[1], (AttributeName{"duz"sv, false}));

      ASSERT_EQ("RETURN SPLIT(@param, '#') ", nested.expression());
      analyzerFeaturesChecker({"position"}, nested.features().value());
      ASSERT_TRUE(nested.overrideValue());

      ASSERT_FALSE(nested.isArray());
      ASSERT_FALSE(nested.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      ASSERT_TRUE(nested.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    }
  }
}

TEST_F(IResearchInvertedIndexMetaTest, testReadCustomizedValues2) {

  auto json = VPackParser::fromJson(complexJsonDefinition2.data(), complexJsonDefinition2.size());

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

  ASSERT_EQ(meta._analyzerDefinitions.size(), 3);
  ASSERT_NE(meta._analyzerDefinitions.find(vocbase.name() + "::delimiter_analyzer"),
            meta._analyzerDefinitions.end());
  ASSERT_NE(meta._analyzerDefinitions.find(vocbase.name() + "::stem_analyzer"),
            meta._analyzerDefinitions.end());
  ASSERT_NE(meta._analyzerDefinitions.find("identity"),
            meta._analyzerDefinitions.end());

  // Check primary sort
  ASSERT_FALSE(meta._sort.empty());
  const auto& primarySortFields = meta._sort.fields();
  ASSERT_EQ(2, primarySortFields.size());
  {
    ASSERT_EQ(1, primarySortFields[0].size());
    ASSERT_EQ(primarySortFields[0][0], (AttributeName{"foo"sv, false}));
  }
  {
    ASSERT_EQ(2, primarySortFields[1].size());
    ASSERT_EQ(primarySortFields[1][0], (AttributeName{"foo"sv, false}));
    ASSERT_EQ(primarySortFields[1][1], (AttributeName{"boo"sv, false}));
  }

  ASSERT_TRUE(meta._sort.direction(0));
  ASSERT_FALSE(meta._sort.direction(1));
  ASSERT_EQ("de_DE_PHONEBOOK", meta._sort.Locale());
  EXPECT_EQ(irs::type<irs::compression::lz4>::id(),
            meta._sort.sortCompression());

  ASSERT_TRUE(meta.dense());

  // Check stored values
  ASSERT_FALSE(meta._storedValues.empty());
  auto storedValues = meta._storedValues.columns();
  ASSERT_EQ(2, storedValues.size());
  {
    auto& storedValuesField0 = storedValues[0];
    ASSERT_FALSE(storedValuesField0.name.empty()); // field name with delimiter. Hard to compare it. Just check emptyness
    ASSERT_EQ(irs::type<irs::compression::lz4>::id(), storedValuesField0.compression().id());
    ASSERT_EQ(1, storedValuesField0.fields.size());
    ASSERT_EQ("foo.boo", storedValuesField0.fields[0].first); // Now verify field name
  }
  {
    auto& storedValuesField1 = storedValues[1];
    ASSERT_FALSE(storedValuesField1.name.empty()); // field name with delimiter. Hard to compare it. Just check emptyness
    ASSERT_EQ(irs::type<irs::compression::lz4>::id(), storedValuesField1.compression().id());
    ASSERT_EQ(1, storedValuesField1.fields.size());
    ASSERT_EQ("foo.goo", storedValuesField1.fields[0].first); // Now verify field name
  }

  ASSERT_EQ(meta._version,
            static_cast<uint32_t>(arangodb::iresearch::LinkVersion::MAX));
  ASSERT_EQ(Consistency::kEventual, meta._consistency);
  ASSERT_TRUE(meta._defaultAnalyzerName.empty()); // FIXME: HOW COME?
  ASSERT_FALSE(meta._features);
  ASSERT_FALSE(meta._includeAllFields);
  ASSERT_TRUE(meta._trackListPositions);

  ASSERT_EQ(meta._fields.size(), 5);

  // Iterating through fields and check them
  {
    auto& field0 = meta._fields[0];

    ASSERT_EQ(1, field0.attribute().size());
    ASSERT_EQ(field0.attribute()[0], (AttributeName{"dummy"sv, false}));

    ASSERT_FALSE(field0.overrideValue());
    ASSERT_EQ(0, field0.expansion().size());
    ASSERT_EQ(0, field0.nested().size());
    ASSERT_EQ(0, field0.expression().size());

    ASSERT_EQ("identity", field0.analyzerName()); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_FALSE(field0.features().has_value()); // Features are not specified in current field
    ASSERT_FALSE(meta._features.has_value()); // Features are not specified in meta
    analyzerFeaturesChecker({"norm", "frequency"}, field0.analyzer()->features()); // FIXME: which features should be defined by default?

    ASSERT_FALSE(field0.isArray());
    ASSERT_TRUE(field0.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field0.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field1 = meta._fields[1];

    ASSERT_EQ(1, field1.attribute().size());
    ASSERT_EQ(field1.attribute()[0], (AttributeName{"foo"sv, false}));

    ASSERT_FALSE(field1.overrideValue());
    ASSERT_EQ(0, field1.expansion().size());
    ASSERT_EQ(0, field1.nested().size());
    ASSERT_EQ("Abc", field1.expression());

    ASSERT_EQ("identity", field1.analyzerName()); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_FALSE(field1.features().has_value()); // Features are not specified in current field
    ASSERT_FALSE(meta._features.has_value()); // Features are not specified in meta
    analyzerFeaturesChecker({"norm", "frequency"}, field1.analyzer()->features());

    ASSERT_FALSE(field1.isArray());
    ASSERT_TRUE(field1.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field1.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field2 = meta._fields[2];

    ASSERT_EQ(2, field2.attribute().size());
    ASSERT_EQ(field2.attribute()[0], (AttributeName{"foo"sv, false}));
    ASSERT_EQ(field2.attribute()[1], (AttributeName{"boo"sv, false}));

    ASSERT_TRUE(field2.overrideValue());
    ASSERT_EQ(0, field2.expansion().size());
    ASSERT_EQ(0, field2.nested().size());
    ASSERT_EQ("", field2.expression());

    ASSERT_EQ(vocbase.name() + "::delimiter_analyzer", field2.analyzerName()); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_FALSE(field2.features().has_value()); // Features are not specified in current field
    ASSERT_FALSE(meta._features.has_value()); // Features are not specified in meta
    analyzerFeaturesChecker({"frequency"}, field2.analyzer()->features());

    ASSERT_FALSE(field2.isArray());
    ASSERT_TRUE(field2.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field2.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field3 = meta._fields[3];

    ASSERT_EQ(2, field3.attribute().size());
    ASSERT_EQ(field3.attribute()[0], (AttributeName{"foo"sv, false}));
    ASSERT_EQ(field3.attribute()[1], (AttributeName{"goo"sv, false}));

    ASSERT_TRUE(field3.overrideValue());
    ASSERT_EQ(0, field3.expansion().size());
    ASSERT_EQ(0, field3.nested().size());
    ASSERT_EQ("", field3.expression());

    ASSERT_EQ(vocbase.name() + "::stem_analyzer", field3.analyzerName()); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_FALSE(field3.features().has_value()); // Features are not specified in current field
    ASSERT_FALSE(meta._features.has_value()); // Features are not specified in meta
    analyzerFeaturesChecker({"norm"}, field3.analyzer()->features());

    ASSERT_FALSE(field3.isArray());
    ASSERT_TRUE(field3.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field3.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
  }

  {
    auto& field4 = meta._fields[4];

    ASSERT_EQ(1, field4.attribute().size());
    ASSERT_EQ(field4.attribute()[1], (AttributeName{"zoo"sv, false}));

    ASSERT_TRUE(field4.overrideValue());
    ASSERT_EQ(0, field4.expansion().size());
    ASSERT_EQ("", field4.expression());

    ASSERT_EQ("identity", field4.analyzerName()); // identity by default. FIXME: Default should be changed by root analyzer
    ASSERT_FALSE(field4.features().has_value()); // Features are not specified in current field
    ASSERT_FALSE(meta._features.has_value()); // Features are not specified in meta
    analyzerFeaturesChecker({"norm", "frequency"}, field4.analyzer()->features());

    ASSERT_TRUE(field4.isArray());
    ASSERT_TRUE(field4.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    ASSERT_FALSE(field4.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT

    ASSERT_EQ(2, field4.nested().size());
    {
      auto& nested0 = field4.nested()[0];
      ASSERT_EQ(1, nested0.attribute().size());
      ASSERT_EQ(nested0.attribute()[0], (AttributeName{"zoo"sv, false}));

      ASSERT_FALSE(nested0.overrideValue());
      ASSERT_EQ(0, nested0.expansion().size());
      ASSERT_EQ(0, nested0.nested().size());
      ASSERT_EQ("", nested0.expression());

      ASSERT_EQ("identity", field4.analyzerName()); // identity by default. FIXME: Default should be changed by root analyzer
      ASSERT_FALSE(field4.features().has_value()); // Features are not specified in current field
      ASSERT_FALSE(meta._features.has_value()); // Features are not specified in meta
      analyzerFeaturesChecker({"norm", "frequency"}, field4.analyzer()->features());

      ASSERT_FALSE(nested0.isArray());
      ASSERT_TRUE(nested0.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      ASSERT_FALSE(nested0.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    }

    {
      auto& nested1 = field4.nested()[1];
      ASSERT_EQ(1, nested1.attribute().size());
      ASSERT_EQ(nested1.attribute()[0], (AttributeName{"doo"sv, false}));

      ASSERT_TRUE(nested1.overrideValue());
      ASSERT_EQ(0, nested1.expansion().size());
      ASSERT_EQ(0, nested1.nested().size());
      ASSERT_EQ("", nested1.expression());

      ASSERT_EQ(vocbase.name() + "::stem_analyzer", nested1.analyzerName()); // identity by default. FIXME: Default should be changed by root analyzer
      ASSERT_FALSE(meta._features.has_value()); // Features are not specified in meta
      ASSERT_TRUE(nested1.features().has_value());
      analyzerFeaturesChecker({"frequency"}, nested1.features().value());

      ASSERT_FALSE(nested1.isArray());
      ASSERT_TRUE(nested1.trackListPositions()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
      ASSERT_TRUE(nested1.includeAllFields()); // FIXME: SHOULD BE DEFAULT VALUE FROM ROOT
    }
  }
}

TEST_F(IResearchInvertedIndexMetaTest, testSerialization) {

  std::array<std::string_view, 2> jsons{complexJsonDefinition1, complexJsonDefinition2};

  for (const auto& json : jsons) {
    serializationChecker(server.server(), json);
  }
}
