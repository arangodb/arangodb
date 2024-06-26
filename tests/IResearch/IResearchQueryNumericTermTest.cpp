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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <absl/strings/str_replace.h>

#include <velocypack/Iterator.h>

#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewSort.h"
#include "IResearchQueryCommon.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "store/mmap_directory.hpp"
#include "utils/index_utils.hpp"

namespace arangodb::tests {
namespace {

static std::vector<std::string> const kEmpty;

// ArangoDB specific string comparer
struct StringComparer {
  bool operator()(std::string_view lhs, std::string_view rhs) const {
    return arangodb::basics::VelocyPackHelper::compareStringValues(
               lhs.data(), lhs.size(), rhs.data(), rhs.size(), true) < 0;
  }
};

class QueryNumericTerm : public QueryTest {
 protected:
  std::deque<std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
      _insertedDocs;

  void create() {
    // add collection_1
    {
      auto collectionJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"collection_1\" }");
      auto logicalCollection1 =
          _vocbase.createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection1);
    }
    // add collection_2
    {
      auto collectionJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"collection_2\" }");
      auto logicalCollection2 =
          _vocbase.createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection2);
    }
  }

  void populateData() {
    auto logicalCollection1 = _vocbase.lookupCollection("collection_1");
    ASSERT_TRUE(logicalCollection1);
    auto logicalCollection2 = _vocbase.lookupCollection("collection_2");
    ASSERT_TRUE(logicalCollection2);

    // populate view with the data
    arangodb::OperationOptions opt;

    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            _vocbase, arangodb::transaction::OperationOriginTestCase{}),
        kEmpty, {logicalCollection1->name(), logicalCollection2->name()},
        kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());

    // insert into collections
    {
      std::filesystem::path resource;
      resource /= std::string_view(arangodb::tests::testResourceDir);
      resource /= std::string_view("simple_sequential.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(
          resource.string());
      auto root = builder.slice();
      ASSERT_TRUE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[]{
          logicalCollection1, logicalCollection2};

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        auto res = trx.insert(collections[i % 2]->name(), doc, opt);
        EXPECT_TRUE(res.ok());

        res = trx.document(collections[i % 2]->name(), res.slice(), opt);
        EXPECT_TRUE(res.ok());
        _insertedDocs.emplace_back(std::move(res.buffer));
        ++i;
      }
    }

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(_vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  void queryTests() {
    // -----------------------------------------------------------------------------
    // --SECTION-- ==
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq == '0' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq == true RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq == false RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq == null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // missing term
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq == -1 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.value == 90.564, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{12, _insertedDocs[12]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value == 90.564 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.value == -32.5, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{16, _insertedDocs[16]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value == -32.5 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq == 2, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{2, _insertedDocs[2]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq == 2 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq == 2.0, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{2, _insertedDocs[2]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq == 2.0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.value == 100.0, TFIDF() ASC, BM25() ASC, d.seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<ptrdiff_t>();
        if (value != 100) {
          continue;
        }
        expectedDocs.emplace(keySlice.getNumber<size_t>(), doc);
      }

      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH 100.0 == "
                                        "d.value SORT BM25(d) ASC, TFIDF(d) "
                                        "ASC, d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION-- !=
    // -----------------------------------------------------------------------------

    // invalid type, unordered
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("name");
        expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq != '0' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // invalid type, unordered
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("name");
        expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq != false RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // invalid type, d.seq DESC
    {
      std::map<ptrdiff_t,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq != null SORT d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // missing term, unordered
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("name");
        expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq != -1 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // existing duplicated term, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        auto const valueSlice = docSlice.get("value");

        if (!valueSlice.isNone() && valueSlice.getNumber<ptrdiff_t>() == 100) {
          continue;
        }

        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value != 100 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // existing unique term, unordered
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("name");
        expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), doc);
      }
      expectedDocs.erase("C");

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq != 2.0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // missing term, seq DESC
    {
      std::vector<std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;

      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const fieldSlice = docSlice.get("value");

        if (!fieldSlice.isNone() &&
            (fieldSlice.isNumber() && -1. == fieldSlice.getNumber<double>())) {
          continue;
        }

        expectedDocs.emplace_back(doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value != -1 SORT d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 ==
                    arangodb::basics::VelocyPackHelper::compare(
                        arangodb::velocypack::Slice((*expectedDoc)->data()),
                        resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // existing duplicated term, TFIDF() ASC, BM25() ASC, seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");

        if (!valueSlice.isNone() && 123 == valueSlice.getNumber<ptrdiff_t>()) {
          continue;
        }

        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<size_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH 123 != d.value SORT TFIDF(d) ASC, BM25(d) "
          "ASC, d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(resultIt.size(), expectedDocs.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION-- <
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq < '0' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq < true RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq < false RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq < null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq < 7, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key >= 7) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq < 7 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq < 0 (less than min term), unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq < 0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq < 31 (less than max term), BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key >= 31) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq < 31 SORT BM25(d), TFIDF(d), d.seq "
          "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value < 0, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() >= 0) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value < 0 SORT BM25(d), TFIDF(d), d.seq "
          "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value < 95, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() >= 95) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value < 95 SORT BM25(d), TFIDF(d), d.seq "
          "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION-- <=
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq <= '0' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq <= true RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq <= false RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq <= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq <= 7, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key > 7) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq <= 7 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq <= 0 (less or equal than min term), unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq <= 0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      EXPECT_TRUE(resultIt.valid());

      auto const resolved = resultIt.value().resolveExternals();
      EXPECT_TRUE(0 ==
                  arangodb::basics::VelocyPackHelper::compare(
                      arangodb::velocypack::Slice(_insertedDocs[0]->data()),
                      resolved, true));

      resultIt.next();
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq <= 31 (less or equal than max term), BM25() ASC, TFIDF() ASC seq
    // DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key > 31) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq <= 31 SORT BM25(d), TFIDF(d), d.seq "
          "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value <= 0, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() > 0) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value <= 0 SORT BM25(d), TFIDF(d), d.seq "
          "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value <= 95, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() > 95) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value <= "
                                        "95 SORT BM25(d), TFIDF(d), d.seq "
                                        "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION-- >
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq > '0' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq > true RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq > false RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq > null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq > 7, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 7) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq > 7 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq > 31 (greater than max term), unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq > 31 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq > 0 (less or equal than min term), BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 0) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 0 SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value > 0, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() <= 0) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value > 0 SORT BM25(d), TFIDF(d), d.seq "
          "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value > 95, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() <= 95) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value > 95 SORT BM25(d), TFIDF(d), d.seq "
          "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION-- >=
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq >= '0' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq >= true RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq >= false RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq >= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= 7, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key < 7) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq >= 7 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq >= 31 (greater than max term), unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq >= 31 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      EXPECT_TRUE(resultIt.valid());

      auto const resolved = resultIt.value().resolveExternals();
      EXPECT_TRUE(0 ==
                  arangodb::basics::VelocyPackHelper::compare(
                      arangodb::velocypack::Slice(_insertedDocs[31]->data()),
                      resolved, true));

      resultIt.next();
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= 0 (less or equal than min term), BM25() ASC, TFIDF() ASC seq
    // DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 0 SORT BM25(d), TFIDF(d), d.seq "
          "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value >= 0, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() < 0) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= 0 SORT BM25(d), TFIDF(d), d.seq "
          "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value > 95, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || valueSlice.getNumber<ptrdiff_t>() < 95) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value >= "
                                        "95 SORT BM25(d), TFIDF(d), d.seq "
                                        "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION--                                                      Range
    // (>,
    // <)
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > '0' AND d.seq < 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > true AND d.seq < 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > false AND d.seq < 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > null AND d.seq < 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq > 7 AND d.name < 18, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 7 || key >= 18) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 7 AND d.seq < 18 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq > 7 AND d.seq < 18, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 7 || key >= 18) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 7.1 AND d.seq < 17.9 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq > 18 AND d.seq < 7 , unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 18 AND d.seq < 7 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq > 7 AND d.seq < 7.0 , unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 7 AND d.seq < 7.0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq > 0 AND d.seq < 31 , TFIDF() ASC, BM25() ASC, d.name DESC
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>,
               StringComparer>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 0 || key >= 31) {
          continue;
        }
        expectedDocs.emplace(
            arangodb::iresearch::getStringRef(docSlice.get("name")), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 0 AND d.seq < 31 SORT tfidf(d), "
          "BM25(d), d.name DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value > 90.564 AND d.value < 300, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<double>();
        if (value <= 90.564 || value >= 300) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value > 90.564 AND d.value < 300 SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value > -32.5 AND d.value < 50, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<double>();
        if (value <= -32.5 || value >= 50) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value > -32.5 AND d.value < 50 SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION--                                                     Range
    // (>=,
    // <)
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= '0' AND d.seq < 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= true AND d.seq < 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= false AND d.seq < 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= null AND d.seq < 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= 7 AND d.seq < 18, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key < 7 || key >= 18) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 7 AND d.seq < 18 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq > 7.1 AND d.seq <= 17.9, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 7 || key >= 18) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 7.1 AND d.seq <= 17.9 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq >= 18 AND d.seq < 7 , unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 18 AND d.seq < 7 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= 7 AND d.seq < 7.0 , unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 7 AND d.seq < 7.0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= 0 AND d.seq < 31 , TFIDF() ASC, BM25() ASC, d.name DESC
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>,
               StringComparer>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key >= 31) {
          continue;
        }
        expectedDocs.emplace(
            arangodb::iresearch::getStringRef(docSlice.get("name")), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 0 AND d.seq < 31 SORT tfidf(d), "
          "BM25(d), d.name DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value >= 90.564 AND d.value < 300, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<double>();
        if (value < 90.564 || value >= 300) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= 90.564 AND d.value < 300 SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value >= -32.5 AND d.value < 50, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<double>();
        if (value < -32.5 || value >= 50) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= -32.5 AND d.value < 50 SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION--                                                     Range (>,
    // <=)
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > '0' AND d.seq <= 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > true AND d.seq <= 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > false AND d.seq <= 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > null AND d.seq <= 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq > 7 AND d.seq <= 18, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 7 || key > 18) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 7 AND d.seq <= 18 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq > 7 AND d.seq <= 17.9, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 7 || key >= 18) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 7.1 AND d.seq <= 17.9 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq > 18 AND d.seq <= 7 , unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 18 AND d.seq <= 7 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq > 7 AND d.seq <= 7.0 , unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 7 AND d.seq <= 7.0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq > 0 AND d.seq <= 31 , TFIDF() ASC, BM25() ASC, d.name DESC
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>,
               StringComparer>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 0 || key > 31) {
          continue;
        }
        expectedDocs.emplace(
            arangodb::iresearch::getStringRef(docSlice.get("name")), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq > 0 AND d.seq <= 31 SORT tfidf(d), "
          "BM25(d), d.name DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value > 90.564 AND d.value <= 300, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<double>();
        if (value <= 90.564 || value > 300) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value > 90.564 AND d.value <= 300 SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value > -32.5 AND d.value <= 50, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<double>();
        if (value <= -32.5 || value > 50) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value > -32.5 AND d.value <= 50 SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION--                                                    Range (>=,
    // <=)
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= '0' AND d.seq <= 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= true AND d.seq <= 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= false AND d.seq <= 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= null AND d.seq <= 15 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= 7 AND d.seq <= 18, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key < 7 || key > 18) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 7 AND d.seq <= 18 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq >= 7.1 AND d.seq <= 17.9, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 7 || key >= 18) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 7.1 AND d.seq <= 17.9 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq >= 18 AND d.seq <= 7 , unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 18 AND d.seq <= 7 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= 7.0 AND d.seq <= 7.0 , unordered
    // will be optimized to d.seq == 7.0
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 7.0 AND d.seq <= 7.0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      auto const resolved = resultIt.value().resolveExternals();
      EXPECT_TRUE(0 ==
                  arangodb::basics::VelocyPackHelper::compare(
                      arangodb::velocypack::Slice(_insertedDocs[7]->data()),
                      resolved, true));

      resultIt.next();
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq > 7 AND d.seq <= 7.0 , unordered
    // behavior same as above
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 7 AND d.seq <= 7.0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      auto const resolved = resultIt.value().resolveExternals();
      EXPECT_TRUE(0 ==
                  arangodb::basics::VelocyPackHelper::compare(
                      arangodb::velocypack::Slice(_insertedDocs[7]->data()),
                      resolved, true));

      resultIt.next();
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= 0 AND d.seq <= 31 , TFIDF() ASC, BM25() ASC, d.name DESC
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>,
               StringComparer>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key > 31) {
          continue;
        }
        expectedDocs.emplace(
            arangodb::iresearch::getStringRef(docSlice.get("name")), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq >= 0 AND d.seq <= 31 SORT tfidf(d), "
          "BM25(d), d.name DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value >= 90.564 AND d.value <= 300, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<double>();
        if (value < 90.564 || value > 300) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= 90.564 AND d.value <= 300 SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value >= -32.5 AND d.value <= 50, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<double>();
        if (value < -32.5 || value > 50) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= -32.5 AND d.value <= 50 SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION--                                                    Range (>=,
    // <=)
    // -----------------------------------------------------------------------------

    // d.seq >= 7 AND d.seq <= 18, unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key < 7 || key > 18) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq IN 7..18 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq >= 7.1 AND d.seq <= 17.9, unordered
    // (will be converted to d.seq >= 7 AND d.seq <= 17)
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key <= 6 || key >= 18) {
          continue;
        }
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq IN 7.1..17.9 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<size_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.seq >= 18 AND d.seq <= 7 , unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq IN 18..7 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= 7 AND d.seq <= 7.0 , unordered
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq IN 7..7.0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      auto const resolved = resultIt.value().resolveExternals();
      EXPECT_TRUE(0 ==
                  arangodb::basics::VelocyPackHelper::compare(
                      arangodb::velocypack::Slice(_insertedDocs[7]->data()),
                      resolved, true));

      resultIt.next();
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= 0 AND d.seq <= 31 , TFIDF() ASC, BM25() ASC, d.name DESC
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>,
               StringComparer>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        if (key > 31) {
          continue;
        }
        expectedDocs.emplace(
            arangodb::iresearch::getStringRef(docSlice.get("name")), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq IN 0..31 SORT tfidf(d), BM25(d), "
          "d.name DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value >= 90.564 AND d.value <= 300, BM25() ASC, TFIDF() ASC seq DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<double>();
        if (value < 90.564 || value > 300) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value IN 90.564..300 SORT BM25(d), "
          "TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.value >= -32.5 AND d.value <= 50, BM25() ASC, TFIDF() ASC seq DESC
    // (will be converted to d.value >= -32 AND d.value <= 50)
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone()) {
          continue;
        }
        auto const value = valueSlice.getNumber<double>();
        if (value < -32 || value > 50) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        auto const key = keySlice.getNumber<size_t>();
        expectedDocs.emplace(key, doc);
      }

      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value IN "
                                        "-32.5..50 SORT BM25(d), TFIDF(d), "
                                        "d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
  }
};

class QueryNumericTermView : public QueryNumericTerm {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }

  void createView() {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");

    // add view
    auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
        _vocbase.createView(createJson->slice(), false));
    ASSERT_FALSE(!view);

    // add link to collection
    {
      auto viewDefinitionTemplate = R"({
      "links": {
        "collection_1": {
          "includeAllFields": true,
          "version":$0 },
        "collection_2": {
          "includeAllFields": true,
          "version":$1 }
      }
    })";

      auto viewDefinition = absl::Substitute(
          viewDefinitionTemplate, static_cast<uint32_t>(linkVersion()),
          static_cast<uint32_t>(linkVersion()));

      auto updateJson = VPackParser::fromJson(viewDefinition);

      EXPECT_TRUE(view->properties(updateJson->slice(), true, true).ok());

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder,
                       arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(slice.get("name").copyString(), "testView");
      EXPECT_TRUE(slice.get("type").copyString() ==
                  arangodb::iresearch::StaticStrings::ViewArangoSearchType);
      EXPECT_TRUE(slice.get("deleted").isNone());  // no system properties
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
    }
  }
};

class QueryNumericTermSearch : public QueryNumericTerm {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }

  void createSearch() {
    // create indexes
    auto createIndex = [this](int name) {
      bool created = false;
      auto createJson = VPackParser::fromJson(absl::Substitute(
          R"({ "name": "index_$0", "type": "inverted",
               "version": $1,
               "includeAllFields": true })",
          name, version()));
      auto collection =
          _vocbase.lookupCollection(absl::Substitute("collection_$0", name));
      ASSERT_TRUE(collection);
      collection->createIndex(createJson->slice(), created).waitAndGet();
      ASSERT_TRUE(created);
    };
    createIndex(1);
    createIndex(2);

    // add view
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"search-alias\" }");

    auto view = std::dynamic_pointer_cast<arangodb::iresearch::Search>(
        _vocbase.createView(createJson->slice(), false));
    ASSERT_FALSE(!view);

    // add link to collection
    {
      auto const viewDefinition = R"({
      "indexes": [
        { "collection": "collection_1", "index": "index_1"},
        { "collection": "collection_2", "index": "index_2"}
      ]})";
      auto updateJson = arangodb::velocypack::Parser::fromJson(viewDefinition);
      auto r = view->properties(updateJson->slice(), true, true);
      EXPECT_TRUE(r.ok()) << r.errorMessage();
    }
  }
};

TEST_P(QueryNumericTermView, Test) {
  create();
  createView();
  populateData();
  queryTests();
}

TEST_P(QueryNumericTermSearch, Test) {
  create();
  createSearch();
  populateData();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryNumericTermView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryNumericTermSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
