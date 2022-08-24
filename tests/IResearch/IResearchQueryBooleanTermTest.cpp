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

namespace arangodb::tests {
namespace {

class QueryBooleanTerm : public QueryTest {
 protected:
  void createCollections() {
    // testCollection0
    {
      auto createJson =
          VPackParser::fromJson("{ \"name\": \"testCollection0\" }");
      auto collection = _vocbase.createCollection(createJson->slice());
      ASSERT_TRUE(collection);

      std::vector<std::shared_ptr<VPackBuilder>> docs{
          VPackParser::fromJson("{ \"seq\": -7 }"),
          VPackParser::fromJson("{ \"seq\": -6, \"value\": false}"),
          VPackParser::fromJson("{ \"seq\": -5, \"value\": true }"),
          VPackParser::fromJson("{ \"seq\": -4, \"value\": true }"),
          VPackParser::fromJson("{ \"seq\": -3, \"value\": true }"),
          VPackParser::fromJson("{ \"seq\": -2, \"value\": false}"),
          VPackParser::fromJson("{ \"seq\": -1, \"value\": true }"),
          VPackParser::fromJson("{ \"seq\": 0, \"value\": true }"),
          VPackParser::fromJson("{ \"seq\": 1, \"value\": false}")};

      OperationOptions options;
      options.returnNew = true;
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::Create(_vocbase), *collection,
          AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : docs) {
        auto res = trx.insert(collection->name(), entry->slice(), options);
        EXPECT_TRUE(res.ok());
        _insertedDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }
    // testCollection1
    {
      auto createJson =
          VPackParser::fromJson("{ \"name\": \"testCollection1\" }");
      auto collection = _vocbase.createCollection(createJson->slice());
      ASSERT_TRUE(collection);

      std::vector<std::shared_ptr<VPackBuilder>> docs{
          VPackParser::fromJson("{ \"seq\": 2, \"value\": true }"),
          VPackParser::fromJson("{ \"seq\": 3, \"value\": false}"),
          VPackParser::fromJson("{ \"seq\": 4, \"value\": true }"),
          VPackParser::fromJson("{ \"seq\": 5, \"value\": true }"),
          VPackParser::fromJson("{ \"seq\": 6, \"value\": false}"),
          VPackParser::fromJson("{ \"seq\": 7, \"value\": false}"),
          VPackParser::fromJson("{ \"seq\": 8 }")};

      OperationOptions options;
      options.returnNew = true;
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::Create(_vocbase), *collection,
          AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : docs) {
        auto res = trx.insert(collection->name(), entry->slice(), options);
        ASSERT_TRUE(res.ok());
        _insertedDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }
  }

  void queryTests() {
    std::vector<VPackSlice> const empty;
    ////////////////////////////////////////////////////////////////////////////
    ///  ==
    ////////////////////////////////////////////////////////////////////////////
    // invalid type
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value == 'true' RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value == 'false' RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value == 0 RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value == 1 RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value == null RETURN d", empty));
    }
    // d.value == true, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        ASSERT_TRUE(docSlice.isObject());
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        ASSERT_TRUE(keySlice.isNumber());
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value == true RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value == false, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value == false RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value == false, BM25(), TFIDF(), d.seq DESC
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value == false SORT BM25(d), TFIDF(d), "
          "d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
    ////////////////////////////////////////////////////////////////////////////
    ///  !=
    ////////////////////////////////////////////////////////////////////////////
    // invalid type
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;

      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const keySlice = docSlice.get("seq");
        auto const fieldSlice = docSlice.get("value");

        if (!fieldSlice.isNone() &&
            (fieldSlice.isString() &&
             "true" == iresearch::getStringRef(fieldSlice))) {
          continue;
        }

        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value != 'true' RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // invalid type
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;

      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const keySlice = docSlice.get("seq");
        auto const fieldSlice = docSlice.get("value");

        if (!fieldSlice.isNone() &&
            (fieldSlice.isString() &&
             "false" == iresearch::getStringRef(fieldSlice))) {
          continue;
        }

        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value != 'false' RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // invalid type
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;

      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const keySlice = docSlice.get("seq");
        auto const fieldSlice = docSlice.get("value");

        if (!fieldSlice.isNone() &&
            (fieldSlice.isNumber() && 0. == fieldSlice.getNumber<double>())) {
          continue;
        }

        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(_vocbase,
                            "FOR d IN testView SEARCH d.value != 0 RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // invalid type
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const keySlice = docSlice.get("seq");
        auto const fieldSlice = docSlice.get("value");

        if (!fieldSlice.isNone() &&
            (fieldSlice.isNumber() && 1. == fieldSlice.getNumber<double>())) {
          continue;
        }

        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(_vocbase,
                            "FOR d IN testView SEARCH d.value != 1 RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // invalid type
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const keySlice = docSlice.get("seq");
        auto const fieldSlice = docSlice.get("value");

        if (!fieldSlice.isNone() && fieldSlice.isNull()) {
          continue;
        }

        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value != null RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value != true, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");

        if (!valueSlice.isNone() &&
            (valueSlice.isBoolean() && valueSlice.getBoolean())) {
          continue;
        }

        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value != true RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value != false, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");

        if (!valueSlice.isNone() &&
            (valueSlice.isBoolean() && !valueSlice.getBoolean())) {
          continue;
        }

        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value != false RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value != false, BM25(), TFIDF(), d.seq DESC
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");

        if (!valueSlice.isNone() &&
            (valueSlice.isBoolean() && !valueSlice.getBoolean())) {
          continue;
        }

        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value != false SORT BM25(d), TFIDF(d), "
          "d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
    ////////////////////////////////////////////////////////////////////////////
    ///  <
    ////////////////////////////////////////////////////////////////////////////
    // invalid type
    {
      EXPECT_TRUE(runQuery("FOR d IN testView SEARCH d.value < 'true' RETURN d",
                           empty));
    }
    // invalid type
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value < 'false' RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value < 0 RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value < 1 RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value < null RETURN d", empty));
    }
    // d.value < true, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(_vocbase,
                            "FOR d IN testView SEARCH d.value < true RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value < false, unordered
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value < false RETURN d", empty));
    }
    // d.value < true, BM25(), TFIDF(), d.seq DESC
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(_vocbase,
                            "FOR d IN testView SEARCH d.value < "
                            "true SORT BM25(d), TFIDF(d), d.seq "
                            "DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
    ////////////////////////////////////////////////////////////////////////////
    ///  <=
    ////////////////////////////////////////////////////////////////////////////
    // invalid type
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value <= 'true' RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value <= 'false' RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value <= 0 RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value <= 1 RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value <= null RETURN d", empty));
    }
    // d.value <= true, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value <= true RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value <= false, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value <= false RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value <= true, BM25(), TFIDF(), d.seq DESC
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value <= true SORT BM25(d), TFIDF(d), "
          "d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
    ////////////////////////////////////////////////////////////////////////////
    ///  >
    ////////////////////////////////////////////////////////////////////////////
    // invalid type
    {
      EXPECT_TRUE(runQuery("FOR d IN testView SEARCH d.value > 'true' RETURN d",
                           empty));
    }
    // invalid type
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value > 'false' RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > 0 RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > 1 RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > null RETURN d", empty));
    }
    // d.value > true, unordered
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > true RETURN d", empty));
    }
    // d.value > false, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value > false RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value > false, BM25(), TFIDF(), d.seq DESC
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value > false SORT BM25(d), TFIDF(d), "
          "d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
    ////////////////////////////////////////////////////////////////////////////
    ///  >=
    ////////////////////////////////////////////////////////////////////////////
    // invalid type
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value >= 'true' RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value >= 'false' RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= 0 RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= 1 RETURN d", empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= null RETURN d", empty));
    }
    // d.value >= true, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value >= true RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value >= false, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.value >= false RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value >= false, BM25(), TFIDF(), d.seq DESC
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= false SORT BM25(d), TFIDF(d), "
          "d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
    ////////////////////////////////////////////////////////////////////////////
    ///  Range(>, <)
    ////////////////////////////////////////////////////////////////////////////
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > 'false'"
                   " and d.value < true RETURN d",
                   empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > 0"
                   " and d.value < true RETURN d",
                   empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > null"
                   " and d.value < true RETURN d",
                   empty));
    }
    // empty range
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > true"
                   " and d.value < false RETURN d",
                   empty));
    }
    // d.value > false AND d.value < true
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > false"
                   " and d.value < true RETURN d",
                   empty));
    }
    // d.value > true AND d.value < true
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > true"
                   " and d.value < true RETURN d",
                   empty));
    }
    ////////////////////////////////////////////////////////////////////////////
    ///  Range(>=, <)
    ////////////////////////////////////////////////////////////////////////////
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= 'false'"
                   " and d.value < true RETURN d",
                   empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= 0"
                   " and d.value < true RETURN d",
                   empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= null"
                   " and d.value < true RETURN d",
                   empty));
    }
    // empty range
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= true"
                   " and d.value < false RETURN d",
                   empty));
    }
    // d.value >= true AND d.value < true
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= true"
                   " and d.value < true RETURN d",
                   empty));
    }
    // d.value >= false AND d.value < true, BM25(d), TFIDF(d), d.seq DESC
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= false AND d.value < true SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
    ////////////////////////////////////////////////////////////////////////////
    ///  Range(>, <=)
    ////////////////////////////////////////////////////////////////////////////
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > 'false'"
                   " and d.value <= true RETURN d",
                   empty));
    }
    // invalid type
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value > 0 and d.value <= true RETURN d",
          empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > null"
                   " and d.value <= true RETURN d",
                   empty));
    }
    // d.value > false AND d.value <= false
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > false"
                   " and d.value <= false RETURN d",
                   empty));
    }
    // empty range
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > true"
                   " and d.value <= false RETURN d",
                   empty));
    }
    // d.value > true AND d.value <= true
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value > true"
                   " and d.value <= true RETURN d",
                   empty));
    }
    // d.value > false AND d.value <= true, BM25(d), TFIDF(d), d.seq DESC
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value > false AND d.value <= true SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
    ////////////////////////////////////////////////////////////////////////////
    ///  Range(>=, <=)
    ////////////////////////////////////////////////////////////////////////////
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= 'false'"
                   " and d.value <= true RETURN d",
                   empty));
    }
    // invalid type
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value >= 0 and d.value <= true RETURN d",
          empty));
    }
    // invalid type
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= null"
                   " and d.value <= true RETURN d",
                   empty));
    }
    // empty range
    {
      EXPECT_TRUE(
          runQuery("FOR d IN testView SEARCH d.value >= true"
                   " and d.value <= false RETURN d",
                   empty));
    }
    // d.value >= false AND d.value <= false, unordered
    {
      std::map<size_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(_vocbase,
                            "FOR d IN testView SEARCH d.value >= "
                            "false and d.value <= false RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("seq");
        auto const key = keySlice.getNumber<ptrdiff_t>();

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value >= true AND d.value <= true, d.seq DESC
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean() || !valueSlice.getBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= true AND d.value <= true SORT "
          "d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
    // d.value >= false AND d.value <= true, BM25(d), TFIDF(d), d.seq DESC
    {
      std::map<ptrdiff_t, VPackSlice> expectedDocs;
      for (auto const& doc : _insertedDocs) {
        VPackSlice docSlice = doc.slice().resolveExternals();
        auto const valueSlice = docSlice.get("value");
        if (!valueSlice.isBoolean()) {
          continue;
        }
        auto const keySlice = docSlice.get("seq");
        expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
      }

      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value >= false AND d.value <= true SORT "
          "BM25(d), TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }
    ////////////////////////////////////////////////////////////////////////////
    ///  Range(>=, =<)
    ////////////////////////////////////////////////////////////////////////////
    // empty range
    // (will be converted to d.value >= 1 AND d.value <= 0)
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.value IN true..false RETURN d", empty));
    }
    // empty range
    // (will be converted to d.seq >= 1 AND d.seq <= 0)
    {
      EXPECT_TRUE(runQuery(
          "FOR d IN testView SEARCH d.seq IN true..false RETURN d", empty));
    }
    // d.value >= false AND d.value <= false, unordered
    // (will be converted to d.value >= 0 AND d.value <= 0)
    {
      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value IN false..false RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(0, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }
    // d.seq >= false AND d.seq <= false, unordered
    // (will be converted to d.seq >= 0 AND d.seq <= 0)
    {
      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq IN false..false RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(1, resultIt.size());
      EXPECT_TRUE(resultIt.valid());

      auto const resolved = resultIt.value().resolveExternals();
      auto expectedDoc = _insertedDocs[7].slice();  // seq == 0
      EXPECT_EQ(0,
                basics::VelocyPackHelper::compare(expectedDoc, resolved, true));

      resultIt.next();
      EXPECT_FALSE(resultIt.valid());
    }
    // d.value >= true AND d.value <= true, d.seq DESC
    // (will be converted to d.value >= 0 AND d.value <= 0)
    {
      auto r = executeQuery(_vocbase,
                            "FOR d IN testView SEARCH d.value IN "
                            "true..true SORT d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(0, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }
    // d.seq >= true AND d.seq <= true, unordered
    // (will be converted to d.seq >= 1 AND d.seq <= 1)
    {
      std::unordered_map<size_t, VPackSlice> expectedDocs{
          {1, _insertedDocs[8].slice()}  // seq == 1
      };

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq IN true..true RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());
      EXPECT_TRUE(resultIt.valid());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto key = resolved.get("seq");
        auto expectedDoc = expectedDocs.find(key.getNumber<size_t>());
        EXPECT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
    // d.value >= false AND d.value <= true, BM25(d), TFIDF(d), d.seq DESC
    // (will be converted to d.value >= 0 AND d.value <= 0)
    {
      auto r = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value IN false..true SORT BM25(d), "
          "TFIDF(d), d.seq DESC RETURN d");
      ASSERT_TRUE(r.result.ok());
      auto result = r.data->slice();
      EXPECT_TRUE(result.isArray());

      VPackArrayIterator resultIt(result);
      EXPECT_EQ(0, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }
    // d.seq >= false AND d.seq <= true, unordered
    // (will be converted to d.seq >= 0 AND d.seq <= 1)
    {
      std::unordered_map<size_t, VPackSlice> expectedDocs{
          {0, _insertedDocs[7].slice()},  // seq == 0
          {1, _insertedDocs[8].slice()}   // seq == 1
      };

      auto r = executeQuery(
          _vocbase, "FOR d IN testView SEARCH d.seq IN false..true RETURN d");
      ASSERT_TRUE(r.result.ok()) << r.result.errorMessage();
      auto slice = r.data->slice();
      ASSERT_TRUE(slice.isArray()) << slice.toString();

      VPackArrayIterator resultIt{slice};
      EXPECT_EQ(expectedDocs.size(), resultIt.size());
      EXPECT_TRUE(resultIt.valid());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto key = resolved.get("seq");
        auto expectedDoc = expectedDocs.find(key.getNumber<size_t>());
        EXPECT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(expectedDoc->second,
                                                           resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
  }
};

class QueryBooleanTermView : public QueryBooleanTerm {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }
};

class QueryBooleanTermSearch : public QueryBooleanTerm {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }
};

TEST_P(QueryBooleanTermView, Test) {
  createCollections();
  createView(R"("trackListPositions": true, "storeValues":"id",)",
             R"("storeValues":"id",)");
  queryTests();
}

TEST_P(QueryBooleanTermView, TestWithoutStoreValues) {
  createCollections();
  createView(R"("trackListPositions": true,)", R"()");
  queryTests();
}

TEST_P(QueryBooleanTermSearch, Test) {
  createCollections();
  createIndexes(R"("trackListPositions": true,)", R"()");
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryBooleanTermView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryBooleanTermSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
