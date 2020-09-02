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

class IResearchQueryInTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryInTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -6, \"value\": null }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -5, \"value\": true }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -4, \"value\": \"abc\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -3, \"value\": 3.14 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
        arangodb::velocypack::Parser::fromJson(
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
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

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
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"includeAllFields\": true, "
        "\"trackListPositions\": true },"
        "\"testCollection1\": { \"includeAllFields\": true }"
        "}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(2, cids.size());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // test array
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value IN [ [ -1, 0 ], [ 1, \"abc\" ] ] "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test array via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d['value'] IN [ [ -1, 0 ], [ 1, \"abc\" ] ] "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test bool
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[1].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH d.value IN [ true ] SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_LT(i, expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                      resolved, true)));
      }
      EXPECT_EQ(i, expected.size());
    }
    //NOT
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH d.value NOT IN [ true ] SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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

  // test bool via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[1].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH d['value'] IN [ true, false ] SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
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
          "FOR d IN testView SEARCH d['value'] NOT IN [ true, false ] SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test numeric
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[8].slice(),
        insertedDocs[11].slice(),
        insertedDocs[13].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH d.value IN [ 123, 1234 ] SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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
          "FOR d IN testView SEARCH d.value NOT IN [ 123, 1234 ] SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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

  // test numeric, limit 2
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[8].slice(),
        insertedDocs[11].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH d.value IN [ 123, 1234 ] SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq LIMIT 2 RETURN d");
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
          "FOR d IN testView SEARCH d.value NOT IN [ 123, 1234 ] SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq LIMIT 2 RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        for (const auto& u : expected) {
          EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(u, resolved, true));
        }
        // this also must not be there (it is not in expected due to LIMIT clause)
        EXPECT_NE(0, arangodb::basics::VelocyPackHelper::compare(insertedDocs[13].slice(), resolved, true));
        ++i;
      }
      EXPECT_EQ(i, 2);
    }
  }

  // test numeric via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[8].slice(),
        insertedDocs[11].slice(),
        insertedDocs[13].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH d['value'] IN [ 123, 1234 ] SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
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
          "FOR d IN testView SEARCH d['value'] NOT IN [ 123, 1234 ] SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test null
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH d.value IN [ null ] SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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
          "FOR d IN testView SEARCH d.value NOT IN [ null ] SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
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

  // test null via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[0].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH d['value'] IN [ null, null ] SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
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
          "FOR d IN testView SEARCH d['value'] NOT IN [ null, null ] SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test object
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value IN [ { \"a\": 7, \"b\": \"c\" } ] "
        "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test object via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[6].slice(),
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d['value'] IN [ {}, { \"a\": 7, \"b\": \"c\" "
        "} ] SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test string
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[2].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH d.value IN [ \"abc\", \"xyz\" ] SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
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
          "FOR d IN testView SEARCH d.value NOT IN [ \"abc\", \"xyz\" ] SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
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

  // test string via []
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        insertedDocs[2].slice(),
    };
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH d['value'] IN [ \"abc\", \"xyz\" ] SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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
          "FOR d IN testView SEARCH d['value'] NOT IN [ \"abc\", \"xyz\" ] SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
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
