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

enum Analyzer : unsigned {
  kAnalyzerIdentity = 1 << 0,
  kAnalyzerTest = 1 << 1,
};

std::string_view toString(Analyzer analyzer) noexcept {
  switch (analyzer) {
    case kAnalyzerIdentity:
      return "identity";
    case kAnalyzerTest:
      return "test_analyzer";
  }
  return "";
}

class QueryOr : public QueryTest {
 protected:
  std::deque<std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
      _insertedDocs;

  void create() {
    // add collection_1
    {
      auto collectionJson =
          VPackParser::fromJson("{ \"name\": \"collection_1\" }");
      auto logicalCollection1 =
          _vocbase.createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection1);
    }
    // add collection_2
    {
      auto collectionJson =
          VPackParser::fromJson("{ \"name\": \"collection_2\" }");
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
    {
      OperationOptions opt;

      transaction::Methods trx(
          transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          kEmpty, {logicalCollection1->name(), logicalCollection2->name()},
          kEmpty, transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collections
      {
        std::filesystem::path resource;
        resource /= std::string_view(tests::testResourceDir);
        resource /= std::string_view("simple_sequential.json");

        auto builder =
            basics::VelocyPackHelper::velocyPackFromFile(resource.string());
        auto root = builder.slice();
        ASSERT_TRUE(root.isArray());

        size_t i = 0;

        std::shared_ptr<LogicalCollection> collections[]{logicalCollection1,
                                                         logicalCollection2};

        for (auto doc : velocypack::ArrayIterator(root)) {
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
          tests::executeQuery(
              _vocbase,
              R"(FOR d IN testView SEARCH 1==1 OPTIONS { waitForSync: true } RETURN d)")
              .result.ok());
    }
  }

  void queryTests(unsigned flags) {
    // tests
    if (flags & kAnalyzerIdentity) {
      std::array const expectedDocs{
          velocypack::Slice{_insertedDocs[0]->data()}};

      tests::checkQuery(
          _vocbase, expectedDocs,
          R"(FOR d IN testView SEARCH d.name == 'A' OR NOT (d.same == 'xyz') SORT d.seq DESC RETURN d)");
      tests::checkQuery(
          _vocbase, expectedDocs,
          R"(FOR d IN testView SEARCH d.name == 'A' OR NOT (d.same IN ['xyz']) SORT d.seq DESC RETURN d)");
      tests::checkQuery(
          _vocbase, expectedDocs,
          R"(FOR d IN testView SEARCH d.name == 'A' OR NOT EXISTS(d.same) SORT d.seq DESC RETURN d)");
      tests::checkQuery(
          _vocbase, expectedDocs,
          R"(FOR d IN testView SEARCH d.name == 'A' OR NOT EXISTS(d.same) OPTIONS { conditionOptimization: "none" } SORT d.seq DESC RETURN d)");
    }

    // d.name == 'A' OR d.name == 'Q', d.seq DESC
    if (flags & kAnalyzerIdentity) {
      std::map<ptrdiff_t, std::shared_ptr<velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("name");
        if (keySlice.isNone()) {
          continue;
        }
        auto const key = arangodb::iresearch::getStringRef(keySlice);
        if (key != "A" && key != "Q") {
          continue;
        }
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.name == 'A' OR d.name == 'Q' SORT d.seq "
          "DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_EQUAL_SLICES(velocypack::Slice(expectedDoc->second->data()),
                            resolved);
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.name == 'X' OR d.same == 'xyz', BM25(d) DESC, TFIDF(d) DESC, d.seq DESC
    if (flags & kAnalyzerIdentity) {
      std::map<ptrdiff_t, std::shared_ptr<velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        velocypack::Slice docSlice(doc->data());
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.name == 'X' OR d.same == 'xyz' SORT "
          "BM25(d) DESC, TFIDF(d) DESC, d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      // Check 1st (the most relevant doc)
      // {"name":"X","seq":23,"same":"xyz", "duplicated":"vczc",
      // "prefix":"bateradsfsfasdf" }
      {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE((0 == basics::VelocyPackHelper::compare(
                              velocypack::Slice(expectedDocs[23]->data()),
                              resolved, true)));
        expectedDocs.erase(23);
      }

      // Check the rest of documents
      auto expectedDoc = expectedDocs.rbegin();
      for (resultIt.next(); resultIt.valid(); resultIt.next()) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE((0 == basics::VelocyPackHelper::compare(
                              velocypack::Slice(expectedDoc->second->data()),
                              resolved, true)));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.name == 'K' OR d.value <= 100 OR d.duplicated == abcd, TFIDF(d) DESC,
    // d.seq DESC
    if (flags & kAnalyzerIdentity) {
      std::array const expectedDocs{
          // {"name":"K","seq":10,"same":"xyz","value":12,"duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[10]->data()),
          // {"name":"$","seq":30,"same":"xyz","duplicated":"abcd","prefix":"abcy"
          // }
          velocypack::Slice(_insertedDocs[30]->data()),
          // {"name":"~","seq":26,"same":"xyz",  "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[26]->data()),
          // {"name":"U","seq":20,"same":"xyz", "prefix":"abc",
          // "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[20]->data()),
          // {"name":"E","seq":4,"same":"xyz","value":100,"duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[4]->data()),
          // {"name":"A","seq":0,"same":"xyz","value":100,"duplicated":"abcd","prefix":"abcd"
          // }
          velocypack::Slice(_insertedDocs[0]->data()),
          // {"name":"Q","seq":16,"same":"xyz", "value":-32.5,
          // "duplicated":"vczc"}
          velocypack::Slice(_insertedDocs[16]->data()),
          // {"name":"P","seq":15,"same":"xyz","value":50,"prefix":"abde"}
          velocypack::Slice(_insertedDocs[15]->data()),
          // {"name":"O","seq":14,"same":"xyz","value":0  }
          velocypack::Slice(_insertedDocs[14]->data()),
          // {"name":"N","seq":13,"same":"xyz","value":1,"duplicated":"vczc"}
          velocypack::Slice(_insertedDocs[13]->data()),
          // {"name":"M","seq":12,"same":"xyz","value":90.564  }
          velocypack::Slice(_insertedDocs[12]->data()),
          // {"name":"L","seq":11,"same":"xyz","value":95 }
          velocypack::Slice(_insertedDocs[11]->data()),
          // {"name":"J","seq":9,"same":"xyz","value":100 }
          velocypack::Slice(_insertedDocs[9]->data()),
          // {"name":"I","seq":8,"same":"xyz","value":100,"prefix":"bcd"  }
          velocypack::Slice(_insertedDocs[8]->data()),
          // {"name":"G","seq":6,"same":"xyz","value":100  }
          velocypack::Slice(_insertedDocs[6]->data()),
          // {"name":"D","seq":3,"same":"xyz","value":12,"prefix":"abcde"}
          velocypack::Slice(_insertedDocs[3]->data()),
      };

      tests::checkQuery(
          _vocbase, expectedDocs,
          "FOR d IN testView SEARCH d.name == 'K' OR d.value <= 100 OR "
          "d.duplicated == 'abcd' SORT TFIDF(d) DESC, d.seq DESC RETURN d");
    }

    // d.name == 'A' OR d.name == 'Q' OR d.same != 'xyz', d.seq DESC
    if (flags & kAnalyzerIdentity) {
      std::map<ptrdiff_t, std::shared_ptr<velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("name");
        if (keySlice.isNone()) {
          continue;
        }
        auto const key = arangodb::iresearch::getStringRef(keySlice);
        if (key != "A" && key != "Q") {
          continue;
        }
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult =
          tests::executeQuery(_vocbase,
                              "FOR d IN testView SEARCH d.name == 'A' OR "
                              "d.name == 'Q' OR d.same != "
                              "'xyz' SORT d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_EQUAL_SLICES(velocypack::Slice(expectedDoc->second->data()),
                            resolved);
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.name == 'F' OR EXISTS(d.duplicated), BM25(d) DESC, d.seq DESC
    if (flags & kAnalyzerIdentity) {
      std::map<ptrdiff_t, std::shared_ptr<velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        velocypack::Slice docSlice(doc->data());
        auto const keySlice = docSlice.get("name");
        if (keySlice.isNone()) {
          continue;
        }
        auto const key = arangodb::iresearch::getStringRef(keySlice);
        if (key != "F" && docSlice.get("duplicated").isNone()) {
          continue;
        }
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.name == 'F' OR EXISTS(d.duplicated) SORT "
          "BM25(d) DESC, d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      // Check 1st (the most relevant doc)
      // {"name":"F","seq":5,"same":"xyz", "value":1234 }
      {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE((0 == basics::VelocyPackHelper::compare(
                              velocypack::Slice(expectedDocs[5]->data()),
                              resolved, true)));
        expectedDocs.erase(5);
      }

      // Check the rest of documents
      auto expectedDoc = expectedDocs.rbegin();
      for (resultIt.next(); resultIt.valid(); resultIt.next()) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE((0 == basics::VelocyPackHelper::compare(
                              velocypack::Slice(expectedDoc->second->data()),
                              resolved, true)));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // d.name == 'D' OR STARTS_WITH(d.prefix, 'abc'), TFIDF(d) DESC, d.seq DESC
    if (flags & kAnalyzerIdentity) {
      std::array const expectedDocs{
          // The most relevant document (satisfied both search conditions)

          // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
          velocypack::Slice(_insertedDocs[3]->data()),

          // Less relevant documents (satisfied STARTS_WITH condition only, has
          // unqiue term in 'prefix' field)

          // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" }
          velocypack::Slice(_insertedDocs[25]->data()),

          // {"name":"U","seq":20,"same":"xyz", "prefix":"abc",
          // "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[20]->data()),
          // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd",
          // "prefix":"abcd" }
          velocypack::Slice(_insertedDocs[0]->data()),

          // The least relevant documents (contain non-unique term 'abcy' in
          // 'prefix' field)

          // {"name":"%","seq":31,"same":"xyz",  "prefix":"abcy"}
          velocypack::Slice(_insertedDocs[31]->data()),
          // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd",
          // "prefix":"abcy" }
          velocypack::Slice(_insertedDocs[30]->data()),
      };

      tests::checkQuery(
          _vocbase, expectedDocs,
          "FOR d IN testView SEARCH d.name == 'D' OR STARTS_WITH(d.prefix, "
          "'abc') SORT TFIDF(d) DESC, d.seq DESC RETURN d");
    }

    // d.name == 'D' OR STARTS_WITH(d.prefix, 'abc'), BM25(d) DESC, d.seq DESC
    if (flags & kAnalyzerIdentity) {
      std::array const expectedDocs{
          // The most relevant document (satisfied both search conditions)

          // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
          velocypack::Slice(_insertedDocs[3]->data()),

          // Less relevant documents (satisfied STARTS_WITH condition only, has
          // unqiue term in 'prefix' field)

          // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" }
          velocypack::Slice(_insertedDocs[25]->data()),
          // {"name":"U","seq":20,"same":"xyz", "prefix":"abc",
          // "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[20]->data()),
          // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd",
          // "prefix":"abcd" }
          velocypack::Slice(_insertedDocs[0]->data()),

          // The least relevant documents (contain non-unique term 'abcy' in
          // 'prefix' field)

          // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
          velocypack::Slice(_insertedDocs[31]->data()),

          // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd",
          // "prefix":"abcy" }
          velocypack::Slice(_insertedDocs[30]->data()),
      };

      tests::checkQuery(
          _vocbase, expectedDocs,
          "FOR d IN testView SEARCH d.name == 'D' OR STARTS_WITH(d.prefix, "
          "'abc') SORT BM25(d) DESC, d.seq DESC RETURN d");
    }

    // d.name == 'D' OR STARTS_WITH(d.prefix, 'abc'), BM25(d) DESC, d.seq DESC,
    // LIMIT 3
    if (flags & kAnalyzerIdentity) {
      std::array const expectedDocs{
          // The most relevant document (satisfied both search conditions)

          // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
          velocypack::Slice(_insertedDocs[3]->data()),

          // Less relevant documents (satisfied STARTS_WITH condition only, has
          // unqiue term in 'prefix' field)

          // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" }
          velocypack::Slice(_insertedDocs[25]->data()),
          // {"name":"U","seq":20,"same":"xyz", "prefix":"abc",
          // "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[20]->data()),
      };

      tests::checkQuery(
          _vocbase, expectedDocs,
          "FOR d IN testView SEARCH d.name == 'D' OR STARTS_WITH(d.prefix, "
          "'abc') SORT BM25(d) DESC, d.seq DESC LIMIT 3 RETURN d");
    }

    // STARTS_WITH(d['prefix'], 'abc') OR EXISTS(d.duplicated) OR d.value < 100
    // OR d.name >= 'Z', BM25(d) DESC, TFIDF(d) DESC, d.seq DESC
    if (flags & kAnalyzerIdentity) {
      std::array const expected = {
          // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" ,
          velocypack::Slice(_insertedDocs[25]->data()),
          // {"name":"~","seq":26,"same":"xyz", "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[26]->data()),
          // {"name":"U","seq":20,"same":"xyz", "prefix":"abc",
          // "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[20]->data()),
          // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
          velocypack::Slice(_insertedDocs[3]->data()),
          // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd",
          // "prefix":"abcd" }
          velocypack::Slice(_insertedDocs[0]->data()),
          // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
          velocypack::Slice(_insertedDocs[31]->data()),
          // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd",
          // "prefix":"abcy" }
          velocypack::Slice(_insertedDocs[30]->data()),
          // {"name":"X","seq":23,"same":"xyz", "duplicated":"vczc",
          // "prefix":"bateradsfsfasdf" }
          velocypack::Slice(_insertedDocs[23]->data()),
          // {"name":"S","seq":18,"same":"xyz", "duplicated":"vczc"}
          velocypack::Slice(_insertedDocs[18]->data()),
          // {"name":"Q","seq":16,"same":"xyz", "value":-32.5,
          // "duplicated":"vczc"}
          velocypack::Slice(_insertedDocs[16]->data()),
          // {"name":"P","seq":15,"same":"xyz","value":50, "prefix":"abde"},
          velocypack::Slice(_insertedDocs[15]->data()),
          // {"name":"O","seq":14,"same":"xyz","value":0 },
          velocypack::Slice(_insertedDocs[14]->data()),
          // {"name":"N","seq":13,"same":"xyz","value":1, "duplicated":"vczc"},
          velocypack::Slice(_insertedDocs[13]->data()),
          // {"name":"M","seq":12,"same":"xyz","value":90.564 },
          velocypack::Slice(_insertedDocs[12]->data()),
          // {"name":"L","seq":11,"same":"xyz","value":95 }
          velocypack::Slice(_insertedDocs[11]->data()),
          // {"name":"K","seq":10,"same":"xyz","value":12, "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[10]->data()),
          // {"name":"H","seq":7,"same":"xyz", "value":123,
          // "duplicated":"vczc"},
          velocypack::Slice(_insertedDocs[7]->data()),
          // {"name":"E","seq":4,"same":"xyz", "value":100, "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[4]->data()),
          // {"name":"C","seq":2,"same":"xyz", "value":123, "duplicated":"vczc"}
          velocypack::Slice(_insertedDocs[2]->data()),
          // {"name":"B","seq":1,"same":"xyz", "value":101, "duplicated":"vczc"}
          velocypack::Slice(_insertedDocs[1]->data()),
      };

      tests::checkQuery(
          _vocbase, expected,
          "FOR d IN testView SEARCH STARTS_WITH(d['prefix'], 'abc') OR "
          "EXISTS(d.duplicated) OR d.value < 100 OR d.name >= 'Z' SORT "
          "TFIDF(d) "
          "DESC, d.seq DESC RETURN d");
    }

    if (flags == (kAnalyzerIdentity | kAnalyzerTest)) {
      std::array const expected = {
          // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" }
          velocypack::Slice(_insertedDocs[25]->data()),
          // {"name":"~","seq":26,"same":"xyz", "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[26]->data()),
          // {"name":"X","seq":23,"same":"xyz", "duplicated":"vczc",
          // "prefix":"bateradsfsfasdf" }
          velocypack::Slice(_insertedDocs[23]->data()),
          // {"name":"S","seq":18,"same":"xyz", "duplicated":"vczc"}
          velocypack::Slice(_insertedDocs[18]->data()),
          // {"name":"Q","seq":16,"same":"xyz", "value":-32.5,
          // "duplicated":"vczc"}
          velocypack::Slice(_insertedDocs[16]->data()),
          // {"name":"N","seq":13,"same":"xyz","value":1, "duplicated":"vczc"},
          velocypack::Slice(_insertedDocs[13]->data()),
          // {"name":"H","seq":7,"same":"xyz", "value":123,
          // "duplicated":"vczc"},
          velocypack::Slice(_insertedDocs[7]->data()),
          // {"name":"C","seq":2,"same":"xyz", "value":123, "duplicated":"vczc"}
          velocypack::Slice(_insertedDocs[2]->data()),
          // {"name":"B","seq":1,"same":"xyz", "value":101, "duplicated":"vczc"}
          velocypack::Slice(_insertedDocs[1]->data()),
          // {"name":"U","seq":20,"same":"xyz", "prefix":"abc",
          // "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[20]->data()),
          // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
          velocypack::Slice(_insertedDocs[3]->data()),
          // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd",
          // "prefix":"abcd" }
          velocypack::Slice(_insertedDocs[0]->data()),
          // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
          velocypack::Slice(_insertedDocs[31]->data()),
          // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd",
          // "prefix":"abcy" }
          velocypack::Slice(_insertedDocs[30]->data()),
          // {"name":"P","seq":15,"same":"xyz","value":50, "prefix":"abde"},
          velocypack::Slice(_insertedDocs[15]->data()),
          // {"name":"O","seq":14,"same":"xyz","value":0  },
          velocypack::Slice(_insertedDocs[14]->data()),
          // {"name":"M","seq":12,"same":"xyz","value":90.564 },
          velocypack::Slice(_insertedDocs[12]->data()),
          // {"name":"L","seq":11,"same":"xyz","value":95 }
          velocypack::Slice(_insertedDocs[11]->data()),
          // {"name":"K","seq":10,"same":"xyz","value":12, "duplicated":"abcd"}
          velocypack::Slice(_insertedDocs[10]->data()),
      };

      tests::checkQuery(
          _vocbase, expected,
          "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 1, "
          "'z'), "
          "'test_analyzer') OR STARTS_WITH(d['prefix'], 'abc') OR d.value < "
          "100 "
          "OR d.name >= 'Z' SORT TFIDF(d) DESC, d.seq DESC RETURN d");
    }
  }
};

class QueryOrView : public QueryOr {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }

  void createView() {
    auto createJson = VPackParser::fromJson(
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
          "analyzers": [ "test_analyzer", "identity" ],
          "includeAllFields": true,
          "trackListPositions": true,
          "version": $0,
          "storeValues":"id" },
        "collection_2": {
          "analyzers": [ "test_analyzer", "identity" ],
          "includeAllFields": true,
          "version": $1,
          "storeValues":"id" }
      }})";

      auto viewDefinition = absl::Substitute(
          viewDefinitionTemplate, static_cast<uint32_t>(linkVersion()),
          static_cast<uint32_t>(linkVersion()));

      auto updateJson = VPackParser::fromJson(viewDefinition);

      EXPECT_TRUE(view->properties(updateJson->slice(), true, true).ok());

      velocypack::Builder builder;

      builder.openObject();
      auto res = view->properties(builder,
                                  LogicalDataSource::Serialization::Properties);
      ASSERT_TRUE(res.ok());
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

class QueryOrSearch : public QueryOr {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }

  void createSearch(Analyzer analyzer) {
    // create indexes
    auto createIndex = [&](int name) {
      bool created = false;
      auto createJson = VPackParser::fromJson(absl::Substitute(
          R"({ "name": "index_$0", "type": "inverted",
               "version": $1,
               "analyzer": "$2",
               "trackListPositions": $3,
               "includeAllFields": true })",
          name, version(), toString(analyzer), (name == 1 ? "true" : "false")));
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

TEST_P(QueryOrView, Test) {
  create();
  createView();
  populateData();
  queryTests(kAnalyzerIdentity | kAnalyzerTest);
}

TEST_P(QueryOrSearch, TestIdentity) {
  create();
  createSearch(kAnalyzerIdentity);
  populateData();
  queryTests(kAnalyzerIdentity);
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryOrView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryOrSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
