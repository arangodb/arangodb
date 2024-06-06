////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
////////////////////////////////////////////////////////////////////////////////

#include <absl/strings/str_replace.h>

#include <velocypack/Iterator.h>

#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewSort.h"
#include "IResearchQueryCommon.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "store/mmap_directory.hpp"
#include "utils/index_utils.hpp"

namespace arangodb::tests {
namespace {

class QueryWildcard : public QueryTest {
 protected:
  void create() {
    // create collection1
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"testCollection1\" }");
      auto collection = _vocbase.createCollection(createJson->slice());
      ASSERT_NE(nullptr, collection);

      std::filesystem::path resource;
      resource /= std::string_view(arangodb::tests::testResourceDir);
      resource /= std::string_view("simple_sequential.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(
          resource.string());
      auto slice = builder.slice();
      ASSERT_TRUE(slice.isArray());

      arangodb::OperationOptions options;
      options.returnNew = true;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          *collection, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto res = trx.insert(collection->name(), itr.value(), options);
        EXPECT_TRUE(res.ok());
        _insertedDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }
  }

  void queryTests() {
    // test missing field
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.missing, '%c%') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0U, slice.length());
    }

    // test missing field via []
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d['missing'] LIKE 'abc' SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");

      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0U, slice.length());
    }

    // test invalid column type
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.seq, '0') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");

      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0U, slice.length());
    }

    // test invalid column type via []
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d['seq'] LIKE '0' SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");

      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0U, slice.length());
    }

    // test invalid input type (empty-array)
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value LIKE [ ] SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (empty-array) via []
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d['value'], [ ]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (array)
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.value, [ 1, \"abc\" ]) SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (array) via []
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d['value'], [ 1, \"abc\" ]) SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (boolean)
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.value, true) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (boolean) via []
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d['value'], false) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (null)
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.value, null) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (null) via []
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d['value'], null) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (numeric)
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.value, 3.14) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (numeric) via []
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d['value'], 1234) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (object)
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.value, { \"a\": 7, \"b\": \"c\" }) "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid input type (object) via []
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d['value'], { \"a\": 7, \"b\": \"c\" "
          "}) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test missing value
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.value) SORT BM25(d) ASC, TFIDF(d) "
          "DESC, d.seq RETURN d");
      ASSERT_TRUE(
          result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
    }

    // test missing value via []
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d['value']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(
          result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
    }

    // test invalid analyzer type (array)
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(LIKE(d.duplicated, 'z'), [ 1, "
          "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // test invalid analyzer type (array) via []
    {
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH ANALYZER(d['duplicated'] LIKE 'z', [ 1, "
          "\"abc\" ]) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // match any
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[0].slice(),  _insertedDocs[3].slice(),
          _insertedDocs[8].slice(),  _insertedDocs[15].slice(),
          _insertedDocs[20].slice(), _insertedDocs[23].slice(),
          _insertedDocs[25].slice(), _insertedDocs[28].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, '%') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // exact match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice()};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, 'abcd') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // prefix match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[0].slice(),  _insertedDocs[3].slice(),
          _insertedDocs[20].slice(), _insertedDocs[25].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, 'abc%') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // prefix match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[0].slice(),  _insertedDocs[3].slice(),
          _insertedDocs[20].slice(), _insertedDocs[25].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, 'abc%%') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // suffix match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[0].slice(),
          _insertedDocs[8].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, '%bcd') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // pattern match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[0].slice(),  _insertedDocs[3].slice(),
          _insertedDocs[8].slice(),  _insertedDocs[20].slice(),
          _insertedDocs[25].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, '%bc%') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // pattern match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[30].slice(), _insertedDocs[31].slice(),
          _insertedDocs[0].slice(),  _insertedDocs[3].slice(),
          _insertedDocs[20].slice(), _insertedDocs[25].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, '_bc%') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // pattern match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[30].slice(),
          _insertedDocs[31].slice(),
          _insertedDocs[0].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, '_bc_') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // pattern match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[3].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, '_bc__') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // pattern match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[3].slice(),
          _insertedDocs[25].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, '_bc__%') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // pattern match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[25].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, '_bc__e_') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // pattern match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[25].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH LIKE(d.prefix, '_bc%_e_') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }
  }
};

class QueryWildcardView : public QueryWildcard {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }

  void createView() {
    // create view
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = _vocbase.createView(createJson->slice(), false);
    ASSERT_FALSE(!logicalView);

    auto* view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto viewDefinitionTemplate = R"({
      "links": {
        "testCollection1": { "includeAllFields": true, "version": $0 }
    }})";

    auto viewDefinition = absl::Substitute(
        viewDefinitionTemplate, static_cast<uint32_t>(linkVersion()));

    auto updateJson = arangodb::velocypack::Parser::fromJson(viewDefinition);

    EXPECT_TRUE(impl->properties(updateJson->slice(), true, true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections(
        [&cids](arangodb::DataSourceId cid, arangodb::LogicalView::Indexes*) {
          cids.emplace(cid);
          return true;
        });
    EXPECT_EQ(1U, cids.size());

    auto const queryString =
        "FOR d IN testView SEARCH 1 ==1 OPTIONS { waitForSync: true } RETURN d";

    // commit data
    EXPECT_TRUE(
        arangodb::tests::executeQuery(_vocbase, queryString).result.ok());
  }
};

class QueryWildcardSearch : public QueryWildcard {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }

  void createSearch() {
    // create index
    {
      bool created = false;
      auto createJson = VPackParser::fromJson(absl::Substitute(
          R"({ "name": "testIndex1", "type": "inverted",
               "version": $0,
               "includeAllFields": true })",
          version()));
      auto collection = _vocbase.lookupCollection("testCollection1");
      ASSERT_TRUE(collection);
      collection->createIndex(createJson->slice(), created).waitAndGet();
      ASSERT_TRUE(created);
    }
    // create view
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"testView\", \"type\": \"search-alias\" }");
      auto logicalView = _vocbase.createView(createJson->slice(), false);
      ASSERT_FALSE(!logicalView);

      auto* view = logicalView.get();
      auto* impl = dynamic_cast<arangodb::iresearch::Search*>(view);
      ASSERT_FALSE(!impl);

      auto viewDefinition = R"({"indexes": [
        {"collection": "testCollection1", "index": "testIndex1"}
      ]})";

      auto updateJson = arangodb::velocypack::Parser::fromJson(viewDefinition);

      EXPECT_TRUE(impl->properties(updateJson->slice(), true, true).ok());
      std::set<arangodb::DataSourceId> cids;
      impl->visitCollections(
          [&cids](arangodb::DataSourceId cid, arangodb::LogicalView::Indexes*) {
            cids.emplace(cid);
            return true;
          });
      EXPECT_EQ(1U, cids.size());

      auto const queryString =
          "FOR d IN testView SEARCH 1==1 OPTIONS {waitForSync: true} RETURN d";

      // commit data
      EXPECT_TRUE(
          arangodb::tests::executeQuery(_vocbase, queryString).result.ok());
    }
  }
};

TEST_P(QueryWildcardView, Test) {
  create();
  createView();
  queryTests();
}

TEST_P(QueryWildcardSearch, Test) {
  create();
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryWildcardView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryWildcardSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
