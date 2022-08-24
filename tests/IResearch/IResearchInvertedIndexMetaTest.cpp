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
void serializationChecker(ArangodServer& server,
                          std::string_view jsonAsString) {
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
    SCOPED_TRACE(::testing::Message("Unexpected error:")
                 << errorString << " in: " << serializedLhs.toString());
    ASSERT_TRUE(res);
  }
  ASSERT_TRUE(errorString.empty());

  VPackBuilder serializedRhs;
  {
    VPackObjectBuilder obj(&serializedRhs);
    ASSERT_TRUE(metaLhs.json(server, serializedRhs, true, &vocbase));
  }
  SCOPED_TRACE(::testing::Message("LHS:")
               << serializedLhs.slice().toString()
               << " RHS:" << serializedRhs.slice().toString());
  ASSERT_EQ(serializedLhs.slice().toString(), serializedRhs.slice().toString());
  ASSERT_EQ(metaLhs, metaRhs);  // FIXME: PrimarySort, StoredValues and etc
                                // should present in metaRhs. At this momemnt we
                                // loose it since serialization works wrong
}
}  // namespace

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
    unused.reset();

    TRI_vocbase_t* vocbase;
    dbFeature.createDatabase(
        testDBInfo(server.server()),
        vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    methods::Collections::createSystem(
        *vocbase, options, tests::AnalyzerCollectionName, false, unused);
    unused.reset();
    auto& analyzers = server.getFeature<IResearchAnalyzerFeature>();
    IResearchAnalyzerFeature::EmplaceResult result;

    analyzers.emplace(
        result, "testVocbase::empty", "empty",
        VPackParser::fromJson("{ \"args\": \"de\" }")->slice(),
        Features(irs::IndexFeatures::FREQ));  // cache the 'empty' analyzer for
                                              // 'testVocbase'
  }
};

TEST_F(IResearchInvertedIndexMetaTest, testWrongDefinitions) {
  // invalid analyzer
  constexpr std::string_view kWrongDefinition2 = R"(
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
  constexpr std::string_view kWrongDefinition3 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "wrong_analyzer"
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
  constexpr std::string_view kWrongDefinition4 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "stem_analyzer"
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
  constexpr std::string_view kWrongDefinition5 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "stem_analyzer"
          }
      ],
      "features": ["features"],
      "analyzerDefinitions": [
          {
              "name": "stem_analyzer",
              "type": "stem",
              "properties": {
                  "locale": "en.utf-8"
              }
          }
      ]
  })";

  // invalid properties for analyzer
  constexpr std::string_view kWrongDefinition6 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "stem_analyzer"
          }
      ],
      "analyzerDefinitions": [
          {
              "name": "stem_analyzer",
              "type": "stem",
              "properties": {
                  "delimiter": "."
              },
              "features": ["features"]
          }
      ]
  })";

  // define field name more than 1 time
  constexpr std::string_view kWrongDefinition7 = R"(
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
  })";  // FIXME: This definition is not crashed

  // only one expansion [*] is allowed
  constexpr std::string_view kWrongDefinition8 = R"(
  {
      "fields": [
          {
              "name": "foo.bar.baz[*].bug.bus[*].bud",
              "analyzer": "stem_analyzer"
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

  // wrong compression in "primarySort"
  constexpr std::string_view kWrongDefinition11 = R"(
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
         "fields": ["foo"],
         "compression": "invalid_compression"
      }
  })";

  // Empty fields array
  constexpr std::string_view kWrongDefinition12 = R"(
  {
      "fields": [],
      "trackListPositions": true,
      "analyzerDefinitions": [
          {
              "name": "stem_analyzer",
              "type": "stem",
              "properties": {
                  "locale": "en.utf-8"
              },
              "features": [
                  "norm"
              ]
          }
      ]
  })";

  // empty object
  constexpr std::string_view kWrongDefinition13 = R"({})";

  // simple definition with more than 1 expansion
  constexpr std::string_view kWrongDefinition14 = R"(
  {
      "fields": [
          "foo.bar[*].buz[*]"
      ]
  })";

  // wrong 'features' field
  constexpr std::string_view kWrongDefinition15 = R"(
  {
      "fields": [
        "foo"
      ],
       "features": "norm",
      "analyzerDefinitions": [
          {
              "name": "stem_analyzer",
              "type": "stem",
              "properties": {
                  "locale": "en.utf-8"
              }
          }
      ]
  })";

  // wrong 'features' field
  constexpr std::string_view kWrongDefinition16 = R"(
  {
      "fields": [
        "foo"
      ],
      "features": [0],
      "analyzerDefinitions": [
          {
              "name": "stem_analyzer",
              "type": "stem",
              "properties": {
                  "locale": "en.utf-8"
              }
          }
      ]
  })";

  // wrong verison
  constexpr std::string_view kWrongDefinition17 = R"(
  {
      "fields": [
          "foo"
      ],
      "version": 42
  })";

  // wrong type of consolidation policy
  constexpr std::string_view kWrongDefinition18 = R"(
  {
      "fields": [
          "foo"
      ],
      "consolidationPolicy": {
        "type" : "abc",
        "segmentsBytesFloor" : 42,
        "segmentsBytesMax" : 42,
        "segmentsMax" : 42,
        "segmentsMin" : 42,
        "minScore" : 42
      }
  })";

  // wrong compression in storedValues
  constexpr std::string_view kWrongDefinition20 = R"(
  {
      "fields": [
          {
              "name": "foo",
              "analyzer": "identity"
          }
      ],
      "storedValues": [{ "fields": ["foo"], "compression": "invalid_compression"}]
  })";

  // array instead of object
  constexpr std::string_view kWrongDefinition21 = R"(["name"])";

  // string instead of object
  constexpr std::string_view kWrongDefinition22 = R"("field_name")";

  // wrong locale in "primarySort"
  constexpr std::string_view kWrongDefinition24 = R"(
  {
      "fields": [
          {
              "name": "foo"
          }
      ],
      "primarySort": {
         "fields": ["foo"],
         "compression": "lz4",
         "locale": "wrong_locale_name"
      }
  })";

  constexpr std::array badJsons{
      kWrongDefinition2,  kWrongDefinition3,  kWrongDefinition4,
      kWrongDefinition5,  kWrongDefinition6,  kWrongDefinition8,
      kWrongDefinition7,  kWrongDefinition11, kWrongDefinition12,
      kWrongDefinition13, kWrongDefinition14, kWrongDefinition15,
      kWrongDefinition16, kWrongDefinition17, kWrongDefinition18,
      kWrongDefinition20, kWrongDefinition21, kWrongDefinition22,
      kWrongDefinition24};

  for (auto jsonD : badJsons) {
    auto json = VPackParser::fromJson(jsonD.data(), jsonD.size());

    arangodb::iresearch::IResearchInvertedIndexMeta meta;
    std::string errorString;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto res = meta.init(server.server(), json->slice(), true, errorString,
                         irs::string_ref(vocbase.name()));
    {
      SCOPED_TRACE(::testing::Message("Definition is not failing: ") << jsonD);
      ASSERT_FALSE(res);
      ASSERT_FALSE(errorString.empty());
    }
  }
}

TEST_F(IResearchInvertedIndexMetaTest, testCorrectDefinitions) {
  // simple definition with 1 expansion
  constexpr std::string_view kDefinition1 = R"(
  {
    "fields": [
      "foo[*]"
    ]
   })";

  // simple definition with 1 expansion
  constexpr std::string_view kDefinition2 = R"(
  {
    "fields": [
      "foo.bar[*]"
    ]
   })";

  // Empty analyzerDefinitions array
  constexpr std::string_view kDefinition3 = R"(
  {
    "fields": ["foo"],
    "analyzerDefinitions":[]
   })";

  // "features" is empty
  constexpr std::string_view kDefinition6 = R"(
  {
    "fields": [
       "simple"
    ],
    "features": []
  })";

  // Duplication of analyzers names
  constexpr std::string_view kDefinition7 = R"(
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

  // empty fields but includeAllFields
  constexpr std::string_view kDefinition8 = R"(
  {
    "fields": [ ],
    "includeAllFields":true,
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

  // missing fields but includeAllFields
  constexpr std::string_view kDefinition9 = R"(
  {
    "includeAllFields":true,
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

  constexpr std::array jsons{kDefinition1, kDefinition2, kDefinition3,
                             kDefinition6, kDefinition7, kDefinition8,
                             kDefinition9};

  for (auto jsonD : jsons) {
    auto json = VPackParser::fromJson(jsonD.data(), jsonD.size());

    arangodb::iresearch::IResearchInvertedIndexMeta meta;
    std::string errorString;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto res = meta.init(server.server(), json->slice(), true, errorString,
                         irs::string_ref(vocbase.name()));
    {
      SCOPED_TRACE(::testing::Message("Definition is failing: ") << jsonD);
      ASSERT_TRUE(res);
      ASSERT_TRUE(errorString.empty());
    }
    serializationChecker(server.server(), jsonD);
  }
}

TEST_F(IResearchInvertedIndexMetaTest,
       testIgnoreAnalyzerDefinitionsUseAnalyzerInMeta) {
  constexpr std::string_view kDefinitionWithAnalyzers = R"(
  {
    "fields": [
      {
        "name": "foo"
      }
    ],
    "analyzer": "myAnalyzer",
    "analyzerDefinitions":[
      {
        "name":"myAnalyzer",
        "type":"stem",
        "properties": {
          "locale": "en.utf-8"
        },
        "features": ["norm"]
      }
    ]
   })";

  auto json = VPackParser::fromJson(kDefinitionWithAnalyzers.data(),
                                    kDefinitionWithAnalyzers.size());

  arangodb::iresearch::IResearchInvertedIndexMeta meta;
  std::string errorString;
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  auto res = meta.init(server.server(), json->slice(), false, errorString,
                       irs::string_ref(vocbase.name()));
  ASSERT_FALSE(res);
}

TEST_F(IResearchInvertedIndexMetaTest,
       testIgnoreAnalyzerDefinitionsUseAnalyzerInField) {
  constexpr std::string_view kDefinitionWithAnalyzers = R"(
  {
    "fields": [
      {
        "name": "foo",
        "analyzer": "myAnalyzer"
      }
    ],
    "analyzerDefinitions":[
      {
        "name":"myAnalyzer",
        "type":"stem",
        "properties": {
          "locale": "en.utf-8"
        },
        "features": ["norm"]
      }
    ]
   })";

  auto json = VPackParser::fromJson(kDefinitionWithAnalyzers.data(),
                                    kDefinitionWithAnalyzers.size());

  arangodb::iresearch::IResearchInvertedIndexMeta meta;
  std::string errorString;
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  auto res = meta.init(server.server(), json->slice(), false, errorString,
                       irs::string_ref(vocbase.name()));
  ASSERT_FALSE(res);
}

TEST_F(IResearchInvertedIndexMetaTest, testIgnoreAnalyzerDefinitions) {
  constexpr std::string_view kDefinitionWithAnalyzers = R"(
  {
    "fields": [
      {
        "name": "foo"
      }
    ],
    "analyzerDefinitions":[
      {
        "name":"myAnalyzer",
        "type":"stem",
        "properties": {
          "locale": "en.utf-8"
        },
        "features": ["norm"]
      }
    ]
   })";

  constexpr std::string_view kDefinitionWithoutAnalyzers = R"(
  {
    "fields": [
      {
        "name": "foo"
      }
    ]
   })";

  arangodb::iresearch::IResearchInvertedIndexMeta metaLhs;
  std::string lhsJsonAsStirng;

  {
    auto json = VPackParser::fromJson(kDefinitionWithAnalyzers.data(),
                                      kDefinitionWithAnalyzers.size());

    std::string errorString;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto res = metaLhs.init(server.server(), json->slice(), false, errorString,
                            irs::string_ref(vocbase.name()));
    {
      SCOPED_TRACE(::testing::Message("Unexpected error in ") << errorString);
      ASSERT_TRUE(res);
    }
    ASSERT_TRUE(errorString.empty());
    ASSERT_EQ(0, metaLhs._analyzerDefinitions.size());

    VPackBuilder b;
    {
      VPackObjectBuilder obj(&b);
      ASSERT_TRUE(metaLhs.json(server.server(), b, false, &vocbase));
    }

    lhsJsonAsStirng = b.slice().toString();
  }

  arangodb::iresearch::IResearchInvertedIndexMeta metaRhs;
  std::string rhsJsonAsStirng;

  {
    auto json = VPackParser::fromJson(kDefinitionWithoutAnalyzers.data(),
                                      kDefinitionWithoutAnalyzers.size());

    std::string errorString;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto res = metaRhs.init(server.server(), json->slice(), false, errorString,
                            irs::string_ref(vocbase.name()));
    {
      SCOPED_TRACE(::testing::Message("Unexpected error in ") << errorString);
      ASSERT_TRUE(res);
    }
    ASSERT_TRUE(errorString.empty());
    ASSERT_EQ(0, metaRhs._analyzerDefinitions.size());

    VPackBuilder b;
    {
      VPackObjectBuilder obj(&b);
      ASSERT_TRUE(metaRhs.json(server.server(), b, false, &vocbase));
    }

    rhsJsonAsStirng = b.slice().toString();
  }

  ASSERT_EQ(metaLhs, metaRhs);
  ASSERT_EQ(lhsJsonAsStirng, rhsJsonAsStirng);
}

TEST_F(IResearchInvertedIndexMetaTest, testDefaults) {
  arangodb::iresearch::IResearchInvertedIndexMeta meta;

  ASSERT_EQ(0, meta._analyzerDefinitions.size());
  ASSERT_TRUE(meta._fields.empty());
  ASSERT_TRUE(meta._sort.empty());
  ASSERT_TRUE(meta._storedValues.empty());
  ASSERT_EQ(Consistency::kEventual, meta._consistency);
  ASSERT_EQ(1, meta._analyzers.size());
  ASSERT_EQ(meta._analyzers[0]._shortName, "identity");
  ASSERT_EQ(meta._features, arangodb::iresearch::Features());
  ASSERT_FALSE(meta._trackListPositions);
  ASSERT_FALSE(meta._includeAllFields);

  ASSERT_EQ(irs::type<irs::compression::lz4>::id(),
            meta._sort.sortCompression());
  ASSERT_FALSE(meta.dense());
  ASSERT_EQ(arangodb::iresearch::LinkVersion::MAX, meta._version);
  ASSERT_EQ(2, meta._cleanupIntervalStep);
  ASSERT_EQ(1000, meta._commitIntervalMsec);
  ASSERT_EQ(1000, meta._consolidationIntervalMsec);
  ASSERT_EQ(0, meta._writebufferActive);
  ASSERT_EQ(64, meta._writebufferIdle);
  ASSERT_EQ(33554432, meta._writebufferSizeMax);

  auto propSlice = meta._consolidationPolicy.properties();
  ASSERT_TRUE(propSlice.isObject());
  {
    ASSERT_TRUE(propSlice.hasKey("type"));
    auto typeSlice = propSlice.get("type");
    ASSERT_TRUE(typeSlice.isString());
    auto type = typeSlice.copyString();
    ASSERT_EQ("tier", type);
  }
  {
    ASSERT_TRUE(propSlice.hasKey("segmentsBytesFloor"));
    auto valueSlice = propSlice.get("segmentsBytesFloor");
    ASSERT_TRUE(valueSlice.isNumber());
    size_t segmentsBytesFloor;
    ASSERT_TRUE(getNumber(segmentsBytesFloor, valueSlice));
    ASSERT_EQ(2097152, segmentsBytesFloor);
  }
  {
    ASSERT_TRUE(propSlice.hasKey("segmentsBytesMax"));
    auto valueSlice = propSlice.get("segmentsBytesMax");
    ASSERT_TRUE(valueSlice.isNumber());
    size_t segmentsBytesMax;
    ASSERT_TRUE(getNumber(segmentsBytesMax, valueSlice));
    ASSERT_EQ(5368709120, segmentsBytesMax);
  }
  {
    ASSERT_TRUE(propSlice.hasKey("segmentsMax"));
    auto typeSlice = propSlice.get("segmentsMax");
    ASSERT_TRUE(typeSlice.isNumber());
    size_t segmentsMax;
    ASSERT_TRUE(getNumber(segmentsMax, typeSlice));
    ASSERT_EQ(10, segmentsMax);
  }
  {
    ASSERT_TRUE(propSlice.hasKey("segmentsMin"));
    auto valueSlice = propSlice.get("segmentsMin");
    ASSERT_TRUE(valueSlice.isNumber());
    size_t segmentsMin;
    ASSERT_TRUE(getNumber(segmentsMin, valueSlice));
    ASSERT_EQ(1, segmentsMin);
  }
  {
    ASSERT_TRUE(propSlice.hasKey("minScore"));
    auto valueSlice = propSlice.get("minScore");
    ASSERT_TRUE(valueSlice.isNumber());
    size_t minScore;
    ASSERT_TRUE(getNumber(minScore, valueSlice));
    ASSERT_EQ(0, minScore);
  }
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
    ASSERT_EQ(irs::type<irs::compression::lz4>::id(),
              meta._sort.sortCompression());
    ASSERT_TRUE(meta._analyzerDefinitions.empty());
    ASSERT_FALSE(meta.dense());
    ASSERT_EQ(arangodb::iresearch::LinkVersion::MAX, meta._version);
    ASSERT_EQ(Consistency::kEventual, meta._consistency);
    ASSERT_FALSE(meta._analyzers.empty());
    ASSERT_EQ(meta._analyzers[0]._shortName, "identity");
    ASSERT_EQ(meta._features, arangodb::iresearch::Features());
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
    ASSERT_EQ(irs::type<irs::compression::lz4>::id(),
              meta._sort.sortCompression());
    ASSERT_TRUE(meta._analyzerDefinitions.empty());
    ASSERT_FALSE(meta.dense());
    ASSERT_EQ(arangodb::iresearch::LinkVersion::MAX, meta._version);
    ASSERT_EQ(Consistency::kEventual, meta._consistency);
    ASSERT_FALSE(meta._analyzers.empty());
    ASSERT_EQ(meta._analyzers[0]._shortName, "identity");
    ASSERT_EQ(meta._features, arangodb::iresearch::Features());
  }
}

TEST_F(IResearchInvertedIndexMetaTest, testDataStoreMetaFields) {
  auto json = VPackParser::fromJson(R"(
    {
      "cleanupIntervalStep" : 2,
      "commitIntervalMsec" : 3,
      "consolidationIntervalMsec" : 4,
      "consolidationPolicy" : {
        "type" : "tier",
        "segmentsBytesFloor" : 5,
        "segmentsBytesMax" : 6,
        "segmentsMax" : 7,
        "segmentsMin" : 8,
        "minScore" : 9
      },
      "version" : 1,
      "writebufferActive" : 10,
      "writebufferIdle" : 11,
      "writebufferSizeMax" : 12,
      "fields": [
        "foo"
      ]
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
  ASSERT_EQ(0, meta._analyzerDefinitions.size());
  ASSERT_EQ(1, meta._fields.size());
  ASSERT_TRUE(meta._sort.empty());
  ASSERT_TRUE(meta._storedValues.empty());
  ASSERT_EQ(meta._sort.sortCompression(),
            irs::type<irs::compression::lz4>::id());
  ASSERT_FALSE(meta.dense());
  ASSERT_EQ(arangodb::iresearch::LinkVersion::MAX, meta._version);
  ASSERT_EQ(meta._consistency, Consistency::kEventual);
  ASSERT_FALSE(meta._analyzers.empty());
  ASSERT_EQ(meta._analyzers[0]._shortName, "identity");
  ASSERT_EQ(meta._features, arangodb::iresearch::Features());
  ASSERT_EQ(meta._writebufferActive, 10);
  ASSERT_EQ(meta._writebufferIdle, 11);
  ASSERT_EQ(meta._writebufferSizeMax, 12);
  VPackBuilder serialized;
  {
    VPackObjectBuilder obj(&serialized);
    ASSERT_TRUE(meta.json(server.server(), serialized, true, &vocbase));
  }
}

#if USE_ENTERPRISE
#include "tests/IResearch/IResearchInvertedIndexMetaTestEE.h"
#endif
