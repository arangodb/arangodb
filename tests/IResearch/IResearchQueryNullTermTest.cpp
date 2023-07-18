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

#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchViewSort.h"
#include "IResearch/VelocyPackHelper.h"
#include "VocBase/LogicalCollection.h"
#include "IResearchQueryCommon.h"
#include "store/mmap_directory.hpp"
#include "utils/index_utils.hpp"

namespace arangodb::tests {
namespace {

class QueryNullTerm : public QueryTest {
 protected:
  void create() {
    // create collection0
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"testCollection0\" }");
      auto collection = _vocbase.createCollection(createJson->slice());
      ASSERT_NE(nullptr, collection);

      std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
          arangodb::velocypack::Parser::fromJson("{ \"seq\": -7 }"),
          arangodb::velocypack::Parser::fromJson(
              "{ \"seq\": -6, \"value\": null}"),
          arangodb::velocypack::Parser::fromJson(
              "{ \"seq\": -5, \"value\": null}"),
          arangodb::velocypack::Parser::fromJson("{ \"seq\": -4 }"),
          arangodb::velocypack::Parser::fromJson(
              "{ \"seq\": -3, \"value\": null}"),
          arangodb::velocypack::Parser::fromJson(
              "{ \"seq\": -2, \"value\": null}"),
          arangodb::velocypack::Parser::fromJson("{ \"seq\": -1 }"),
          arangodb::velocypack::Parser::fromJson(
              "{ \"seq\": 0, \"value\": null }"),
          arangodb::velocypack::Parser::fromJson("{ \"seq\": 1 }")};

      arangodb::OperationOptions options;
      options.returnNew = true;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(_vocbase),
          *collection, arangodb::AccessMode::Type::WRITE,
          arangodb::transaction::TrxType::kInternal);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : docs) {
        auto res = trx.insert(collection->name(), entry->slice(), options);
        EXPECT_TRUE(res.ok());
        _insertedDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }

    // create collection1
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"testCollection1\" }");
      auto collection = _vocbase.createCollection(createJson->slice());
      ASSERT_NE(nullptr, collection);

      std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
          arangodb::velocypack::Parser::fromJson(
              "{ \"seq\": 2, \"value\": null}"),
          arangodb::velocypack::Parser::fromJson("{ \"seq\": 3 }"),
          arangodb::velocypack::Parser::fromJson("{ \"seq\": 4 }"),
          arangodb::velocypack::Parser::fromJson("{ \"seq\": 5 }"),
          arangodb::velocypack::Parser::fromJson(
              "{ \"seq\": 6, \"value\": null}"),
          arangodb::velocypack::Parser::fromJson(
              "{ \"seq\": 7, \"value\": null}"),
          arangodb::velocypack::Parser::fromJson("{ \"seq\": 8 }")};

      arangodb::OperationOptions options;
      options.returnNew = true;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(_vocbase),
          *collection, arangodb::AccessMode::Type::WRITE,
          arangodb::transaction::TrxType::kInternal);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : docs) {
        auto res = trx.insert(collection->name(), entry->slice(), options);
        EXPECT_TRUE(res.ok());
        _insertedDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }
  }

  void queryTests() {
    // -----------------------------------------------------------------------------
    // --SECTION-- ==
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value == 'null' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value == 0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // d.value == null, unordered
    {
      std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");

        if (valueSlice.isNone() || !valueSlice.isNull()) {
          continue;
        }

        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value == null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.value == null, BM25(), TFIDF(), d.seq DESC
    {
      std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");

        if (valueSlice.isNone() || !valueSlice.isNull()) {
          continue;
        }

        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value == null SORT BM25(d), TFIDF(d), "
          "d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION-- !=
    // -----------------------------------------------------------------------------

    // invalid type
    {
      std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const keySlice = docSlice.get("seq");
        auto const fieldSlice = docSlice.get("value");

        if (!fieldSlice.isNone() &&
            "null" == arangodb::iresearch::getStringRef(fieldSlice)) {
          continue;
        }

        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value != 'null' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // invalid type
    {
      std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const keySlice = docSlice.get("seq");
        auto const fieldSlice = docSlice.get("value");

        if (!fieldSlice.isNone() &&
            (fieldSlice.isNumber() && 0. == fieldSlice.getNumber<double>())) {
          continue;
        }

        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value != 0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.value != null, unordered
    {
      std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");

        if (!valueSlice.isNone() && valueSlice.isNull()) {
          continue;
        }

        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value != null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.value != null, BM25(), TFIDF(), d.seq DESC
    {
      std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");

        if (!valueSlice.isNone() && valueSlice.isNull()) {
          continue;
        }

        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value != null SORT BM25(d), TFIDF(d), "
          "d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
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
          _vocbase, "FOR d IN testView SEARCH d.value < 'null' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value < false RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value < 0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // d.value < null
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value < null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // -----------------------------------------------------------------------------
    // --SECTION-- <=
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value <= 'null' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value <= false RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value <= 0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // d.value <= null, unordered
    {
      std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || !valueSlice.isNull()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value <= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.value <= null, BM25(), TFIDF(), d.seq DESC
    {
      std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || !valueSlice.isNull()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value <= null SORT BM25(d), TFIDF(d), "
          "d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
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
          _vocbase, "FOR d IN testView SEARCH d.value > 'null' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value > false RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value > 0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // d.value > null
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value > null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // -----------------------------------------------------------------------------
    // --SECTION-- >=
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value >= 'null' RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value >= 0 RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value >= false RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // d.value >= null, unordered
    {
      std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || !valueSlice.isNull()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value >= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.value >= null, BM25(), TFIDF(), d.seq DESC
    {
      std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || !valueSlice.isNull()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= null SORT BM25(d), TFIDF(d), "
          "d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION-- Range(>,
    // <)
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value > "
                                        "'null' and d.value < null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value > 0 and d.value < null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value > "
                                        "false and d.value < null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // empty range
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value > "
                                        "null and d.value < null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // -----------------------------------------------------------------------------
    // --SECTION-- Range(>=,
    // <)
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value >= "
                                        "'null' and d.value < null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= 0 and d.value < null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value >= "
                                        "false and d.value < null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // empty range
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value >= "
                                        "null and d.value < null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // -----------------------------------------------------------------------------
    // --SECTION--                                                      Range(>,
    // <=)
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value > "
                                        "'null' and d.value <= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value > 0 and d.value <= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value > "
                                        "false and d.value <= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // empty range
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value > "
                                        "null and d.value <= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // -----------------------------------------------------------------------------
    // --SECTION--                                                     Range(>=,
    // <=)
    // -----------------------------------------------------------------------------

    // invalid type
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value >= "
                                        "'null' and d.value <= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= 0 and d.value <= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // invalid type
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value >= "
                                        "false and d.value <= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        IRS_IGNORE(actualDoc);
        EXPECT_TRUE(false);
      }
    }

    // d.value >= null and d.value <= null, unordered
    {
      std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || !valueSlice.isNull()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d.value >= "
                                        "null and d.value <= null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.value >= null and d.value <= null, BM25(), TFIDF(), d.seq DESC
    // (will be converted to d.value >= 0 AND d.value <= 0)
    {
      std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (valueSlice.isNone() || !valueSlice.isNull()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= null and d.value <= null SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // -----------------------------------------------------------------------------
    // --SECTION--                                                     Range(>=,
    // <=)
    // -----------------------------------------------------------------------------

    // d.value >= null and d.value <= null, unordered
    // (will be converted to d.value >= 0 AND d.value <= 0)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value IN null..null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.seq >= nullptr AND d.seq <= nullptr, unordered
    // (will be converted to d.seq >= 0 AND d.seq <= 0)
    {
      std::unordered_map<size_t, arangodb::velocypack::Slice> expectedDocs{
          {0, _insertedDocs[7].slice()},  // seq == 0
      };

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq IN null..null RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());
      EXPECT_TRUE(resultIt.valid());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto key = resolved.get("seq");
        auto expectedDoc = expectedDocs.find(key.getNumber<size_t>());
        EXPECT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             expectedDoc->second, resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // d.value >= null and d.value <= null, BM25(), TFIDF(), d.seq DESC
    // (will be converted to d.value >= 0 AND d.value <= 0)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value IN null..null SORT BM25(d), "
          "TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }
  }
};

class QueryNullTermView : public QueryNullTerm {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }
};

class QueryNullTermSearch : public QueryNullTerm {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }
};

TEST_P(QueryNullTermView, Test) {
  create();
  createView(R"("trackListPositions": true,)", R"()");
  queryTests();
}

TEST_P(QueryNullTermSearch, Test) {
  create();
  createIndexes(R"("trackListPositions": true,)", R"()");
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryNullTermView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryNullTermSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
