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

class IResearchQueryLevenhsteinMatchTest : public IResearchQueryTest {};

}  // namespace

TEST_F(IResearchQueryLevenhsteinMatchTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;
  arangodb::LogicalCollection* collection;

  // create collection1
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\" }");
    collection = vocbase.createCollection(createJson->slice()).get();
    ASSERT_NE(nullptr, collection);
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
        "  \"testCollection1\": { \"includeAllFields\": true } "
        "}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(1, cids.size());
  }

  // insert some data
  {
    irs::utf8_path resource;
    resource /= irs::string_ref(arangodb::tests::testResourceDir);
    resource /= irs::string_ref("levenshtein_sequential.json");

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

    // commit data
    EXPECT_TRUE(trx.commit().ok());

    std::string const queryString =
      "FOR d IN testView SEARCH 1 ==1 OPTIONS { waitForSync: true } RETURN d";
    EXPECT_TRUE(arangodb::tests::executeQuery(vocbase, queryString).result.ok());
  }

  // distance 0, default limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[26].slice()
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'aa', 0) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 1, defatul limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'a', 1) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 1, limit 1
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[27].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'a', 1, false, 1) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 1, no limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'a', 1, false, 0) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 1, default limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'cba', 1, false) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 1, default limit, damerau
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'cba', 1, true) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 1, default limit, default damerau
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[29].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'cba', 1) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 2, defatul limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[31].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'aa', 2) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 2, no limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[31].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'aa', 2, false, 0) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 2, limit 1
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[26].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'aa', 2, false, 1) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 3, default limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[2].slice(),
      insertedDocs[4].slice(),
      insertedDocs[6].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[31].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'ababab', 3, false) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 3, no limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[2].slice(),
      insertedDocs[4].slice(),
      insertedDocs[6].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[31].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'ababab', 3, false, 0) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 3, no limit, SORT
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'ababab', 3, false, 0) "
      "SORT TFIDF(d) DESC "
      "LIMIT 1 "
      "RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 3, limit 1
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, 'ababab', 3, false, 1) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 4, no limit
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[30].slice(),
      insertedDocs[31].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, '', 4, false, 0) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // distance 4, limit 2
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
    };
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, '', 4, false, 2) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    ASSERT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto const resolved = itr.value().resolveExternals();
      ASSERT_TRUE(i < expected.size());
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i],
        resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }


// FIXME
//  // distance 4, no limit, SORT
//  {
//    std::vector<arangodb::velocypack::Slice> expected = {
//      insertedDocs[27].slice(),
//      insertedDocs[28].slice(),
//    };
//    auto result = arangodb::tests::executeQuery(
//      vocbase,
//      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.title, '', 4, false, 0) "
//      "SORT BM25(d) DESC, d.title ASC "
//      "LIMIT 2"
//      "RETURN d");
//    ASSERT_TRUE(result.result.ok());
//    auto slice = result.data->slice();
//    ASSERT_TRUE(slice.isArray());
//    ASSERT_EQ(expected.size(), slice.length());
//    size_t i = 0;
//
//    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
//      auto const resolved = itr.value().resolveExternals();
//      ASSERT_TRUE(i < expected.size());
//      EXPECT_EQUAL_SLICES(expected[i], resolved);
//    }
//
//    EXPECT_EQ(i, expected.size());
//  }

  // test missing field
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.missing, 'alphabet', 3) RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }

  // test missing field via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['missing'], 'abc', 2) RETURN d");

    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }

  // test invalid field type
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.seq, '0', 2) RETURN d");

    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }

  // test invalid field type via []
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['seq'], '0', 2) RETURN d");

    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }

  // test invalid 2nd argument type (empty-array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, [ ], 2) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 2nd argument type (empty-array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], [ ] , 2) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 2nd argument type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, [ 1, \"abc\" ], 2) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 2nd argument type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], false, 2) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 2nd argument type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, null, 2) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 2nd argument type (numeric)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 3.14, 1) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 2nd argument type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, { \"a\": 7, \"b\": \"c\" }, 2) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 3rd argument type (empty-array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', '2') RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 3rd argument type (empty-array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], 'foo' , []) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 3rd argument type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', [2]) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 3rd argument type (boolean) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], 'foo', false) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 3rd argument type (string) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], 'foo', '2') RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 3rd argument type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', null) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 3rd argument type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', { \"a\": 7, \"b\": \"c\" }) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (empty-array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 2, []) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (empty-array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], 'foo', 2, []) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 2, [false]) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (numeric) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], 'foo', 2, 3.14) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (string) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], 'foo', 2, 'false') RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 1, null) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 2, { \"a\": 7, \"b\": \"c\" }) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 5th argument type (empty-array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 2, true, []) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 5th argument type (empty-array) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], 'foo', 2, true, []) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 2, true, [42]) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (bool) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], 'foo', 2, true, false) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (string) via []
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d['value'], 'foo', 2, true, '42') RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (null)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 1, true, null) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid 4th argument type (object)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 2, true, { \"a\": 7, \"b\": \"c\" }) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test max Levenshtein distance
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 5, false) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test max Damerau-Levenshtein distance
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 4, true) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test max Damerau-Levenshtein distance
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 4) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test missing value
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value) SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test missing value
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo') RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test redundant args
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH LEVENSHTEIN_MATCH(d.value, 'foo', 2, true, 42, null) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH));
  }

  // test invalid analyzer type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(LEVENSHTEIN_MATCH(d.duplicated, 'z', 2), [ 1, 'abc' ]) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid analyzer type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(LEVENSHTEIN_MATCH(d['duplicated'], 'z', 2), [ 1, 'abc' ]) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // test invalid boost type (array)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH Boost(LEVENSHTEIN_MATCH(d['duplicated'], 'z', 2), [ 1, 'abc' ]) RETURN d");
    ASSERT_TRUE(result.result.is(TRI_ERROR_BAD_PARAMETER));
  }
}
