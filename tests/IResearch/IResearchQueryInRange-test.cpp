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

#include "IResearchQueryCommon.h"

#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>

extern const char* ARGV0;  // defined in main.cpp

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryInRangeTest : public IResearchQueryTest {};

}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryInRangeTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  std::shared_ptr<arangodb::LogicalCollection> collection0;
  std::shared_ptr<arangodb::LogicalCollection> collection1;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);
    collection0 = collection;

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        VPackParser::fromJson(
            "{ \"seq\": -6, \"value\": null }"),
        VPackParser::fromJson(
            "{ \"seq\": -5, \"value\": true }"),
        VPackParser::fromJson(
            "{ \"seq\": -4, \"value\": \"abc\" }"),
        VPackParser::fromJson(
            "{ \"seq\": -3, \"value\": [ 3.14, -3.14 ] }"),
        VPackParser::fromJson(
            "{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
        VPackParser::fromJson(
            "{ \"seq\": -1, \"value\": { \"a\": 7, \"b\": \"c\" } }"),
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (auto& entry : docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE(trx.commit().ok());
  }

  // create collection1
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testCollection1\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);
    collection1 = collection;

    irs::utf8_path resource;
    resource /= irs::string_ref(arangodb::tests::testResourceDir);
    resource /= irs::string_ref("simple_sequential.json");

    auto builder =
        arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
    auto slice = builder.slice();
    ASSERT_TRUE(slice.isArray());

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto res = trx.insert(collection->name(), itr.value(), options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE(trx.commit().ok());
  }

  // create view
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = VPackParser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"trackListPositions\": "
        "false, \"storeValues\":\"id\" },"
        "\"testCollection1\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"storeValues\":\"id\" }"
        "}}");

    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(2, cids.size());
    EXPECT_TRUE((arangodb::iresearch::IResearchLinkHelper::find(*collection0, *view)
                     ->commit()
                     .ok()));
    EXPECT_TRUE((arangodb::iresearch::IResearchLinkHelper::find(*collection1, *view)
                     ->commit()
                     .ok()));
  }

  // d.value > false && d.value <= true
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[1].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.value, false, true, false, true) "
          "SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();

        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.value, false, true, false, true)) "
          "SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.value >= null && d.value <= null
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.value, null, null, true, true) "
          "SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.value, null, null, true, true)) "
          "SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.value > null && d.value <= null
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    {
      auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH IN_RANGE(d.value, null, null, false, true) "
        "SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.value, null, null, false, true)) "
          "SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.name >= 'A' && d.name <= 'A'
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'A', true, true) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'A', 'A', true, true)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.name >= 'B' && d.name <= 'A'
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.name, 'B', 'A', true, true) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'B', 'A', true, true)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.name >= 'A' && d.name <= 'E'
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'E', true, true) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'A', 'E', true, true)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.name >= 'A' && d.name < 'E'
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),
        insertedDocs[7].slice(),
        insertedDocs[8].slice(),
        insertedDocs[9].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'E', true, false) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'A', 'E', true, false)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.name > 'A' && d.name <= 'E'
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),
        insertedDocs[8].slice(),
        insertedDocs[9].slice(),
        insertedDocs[10].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'E', false, true) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'A', 'E', false, true)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.name > 'A' && d.name < 'E'
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),
        insertedDocs[8].slice(),
        insertedDocs[9].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'E', false, false) "
          "SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'A', 'E', false, false)) "
          "SORT d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.seq >= 5 && d.seq <= -1
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.seq, 5, -1, true, true) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.seq, 5, -1, true, true)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.seq >= 1 && d.seq <= 5
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[9].slice(),  insertedDocs[10].slice(),
        insertedDocs[11].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.seq, 1, 5, true, true) SORT d.seq "
          "RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.seq, 1, 5, true, true)) SORT d.seq "
          "RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.seq > -2 && d.seq <= 5
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[5].slice(),  insertedDocs[6].slice(),
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[9].slice(),  insertedDocs[10].slice(),
        insertedDocs[11].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.seq, -2, 5, false, true) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.seq, -2, 5, false, true)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.seq > 1 && d.seq < 5
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[8].slice(),
        insertedDocs[9].slice(),
        insertedDocs[10].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.seq, 1, 5, false, false) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.seq, 1, 5, false, false)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.seq >= 1 && d.seq < 5
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[7].slice(),
        insertedDocs[8].slice(),
        insertedDocs[9].slice(),
        insertedDocs[10].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.seq, 1, 5, true, false) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.seq, 1, 5, true, false)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.value > 3 && d.value < 4
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[3].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.value, 3, 4, false, false) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.value, 3, 4, false, false)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }

  // d.value > -4 && d.value < -3
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[3].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH IN_RANGE(d.value, -4, -3, false, false) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                 resolved, true));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH NOT(IN_RANGE(d.value, -4, -3, false, false)) SORT "
          "d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        ++i;
      }
      EXPECT_EQ(i, (insertedDocs.size() - expected.size()));
    }
  }
}
