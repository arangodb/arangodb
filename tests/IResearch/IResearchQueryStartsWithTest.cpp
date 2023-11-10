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

#include <absl/strings/str_replace.h>

#include <velocypack/Iterator.h>

#include "Aql/OptimizerRulesFeature.h"
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

class QueryStartsWith : public QueryTest {
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
    // invalid field
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH STARTS_WITH(d.invalid_field, 'abc') RETURN "
          "d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid field via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH STARTS_WITH(d.invalid_field, ['abc', "
          "'def']) "
          "RETURN "
          "d");
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
          "FOR d IN testView SEARCH STARTS_WITH(d.seq, '0') RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid type via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH STARTS_WITH(d.seq, ['0', '1']) RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // execution outside arangosearch empty
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase, "RETURN starts_with()");
      ASSERT_FALSE(queryResult.result.ok());
    }

    // execution outside arangosearch one parameter
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase, "RETURN starts_with('abc')");
      ASSERT_FALSE(queryResult.result.ok());
    }

    // execution outside arangosearch five parameters
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', 'a', 1, 2, 3)");
      ASSERT_FALSE(queryResult.result.ok());
    }

    // execution outside arangosearch five parameters via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['a', 'ab'], 1, 2, 3)");
      ASSERT_FALSE(queryResult.result.ok());
    }

    // execution outside arangosearch (true)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', 'a')");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['a', 'ab'])");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via expresssion
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "LET x = NOOPT(['a', 'ab']) RETURN starts_with('abc', x)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via expresssion
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "LET x = NOOPT(['a', 'ab']) RETURN starts_with('abc', x, 2)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (false) via expresssion
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "LET x = NOOPT(['a', 'b']) RETURN starts_with('abc', x, 2)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_FALSE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via expresssion
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "LET x = NOOPT(['a', 'b']) RETURN starts_with('abc', x, 1)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', 'abc')");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['abc', 'def'])");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (false)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('a', 'abc')");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_FALSE(resolved.getBool());
      }
    }

    // execution outside arangosearch (false) via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('a', ['abc', 'ab'])");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_FALSE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via [] empty array
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', [])");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_FALSE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via [] empty array min match count
    // 0
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', [], 0)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via [] min match count 0, 1 not
    // success
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['b', 'd'], 0)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via [] min match count 1, 1 success
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['a', 'd'], 0)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via [] min match count 1
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['a', 'd'], 1)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (false) via [] min match count 1
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['b', 'd'], 1)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_FALSE(resolved.getBool());
      }
    }

    // execution outside arangosearch (true) via [] min match count = length
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['a', 'ab'], 2)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_TRUE(resolved.getBool());
      }
    }

    // execution outside arangosearch (false) via [] min match count = length
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['a', 'd'], 2)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_FALSE(resolved.getBool());
      }
    }

    // execution outside arangosearch (false) via [] min match count > length 2
    // not success
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['b', 'd'], 3)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_FALSE(resolved.getBool());
      }
    }

    // execution outside arangosearch (false) via [] min match count > length 2
    // success
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('abc', ['a', 'ab'], 3)");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isBool());
        ASSERT_FALSE(resolved.getBool());
      }
    }

    // execution outside arangosearch (wrong args)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with(1, 'abc')");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args) via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with(1, ['abc', 'def'])");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with(true, 'abc')");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args) via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with(true, ['abc', 'def'])");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with(null, 'abc')");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args) via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with(null, ['abc', 'def'])");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args)
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase, "RETURN starts_with('a', 1)");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args) via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('a', [1, 2])");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('a', null)");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args) via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('a', [null])");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('a', true)");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // execution outside arangosearch (wrong args) via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "RETURN starts_with('a', [true, false])");
      ASSERT_TRUE(queryResult.result.ok());
      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());
      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(1U, resultIt.size());
      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        ASSERT_TRUE(resolved.isNull());
      }
    }

    // exact term, unordered
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[0]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.name, 'A') RETURN d");
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

    // exact term, unordered via []
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[0]}, {"B", _insertedDocs[1]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.name, ['A', 'B']) RETURN d");
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

    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[0]}, {"B", _insertedDocs[1]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "LET x = NOOPT(['A', 'B']) FOR d IN testView SEARCH "
          "starts_with(d.name, x) RETURN d");
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

    // invalid prefix
    {
      auto queryResult =
          arangodb::tests::executeQuery(_vocbase,
                                        "LET x = NOOPT([1, 'B']) FOR d IN "
                                        "testView SEARCH starts_with(d.name, "
                                        "x) RETURN d");
      ASSERT_FALSE(queryResult.result.ok());
      ASSERT_EQ(TRI_ERROR_BAD_PARAMETER, queryResult.result.errorNumber());
    }

    // exact term, unordered via [] min match count = 1
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[0]}, {"B", _insertedDocs[1]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.name, ['A', 'B'], 1) RETURN "
          "d");
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

    // exact term, ordered
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[0]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.name, 'A', 0) SORT TFIDF(d) "
          "DESC RETURN d");
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

    // exact term, ordered via []
    {
      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[0]}, {"B", _insertedDocs[1]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.name, ['A', 'B'], 1, 0) SORT "
          "TFIDF(d) "
          "DESC RETURN d");
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

    // d.prefix = abc*, d.seq DESC
    {
      std::map<ptrdiff_t,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const prefixSlice = docSlice.get("prefix");
        if (prefixSlice.isNone() ||
            !arangodb::iresearch::getStringRef(prefixSlice)
                 .starts_with("abc")) {
          continue;
        }
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, 'abc') SORT d.seq "
          "DESC "
          "RETURN d");
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

    // d.prefix = abc*|def*, d.seq DESC via []
    {
      std::map<ptrdiff_t,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const prefixSlice = docSlice.get("prefix");
        if (prefixSlice.isNone() ||
            !arangodb::iresearch::getStringRef(prefixSlice)
                 .starts_with("abc")) {
          continue;
        }
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, ['abc', 'def']) SORT "
          "d.seq DESC "
          "RETURN d");
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

    // d.prefix = empty array, d.seq DESC via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, []) SORT d.seq DESC "
          "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.prefix = empty array, d.seq DESC via [] min match count 0
    {
      std::map<ptrdiff_t,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, [], 0) SORT d.seq "
          "DESC "
          "RETURN d");
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

    // d.prefix = bca*|def*, d.seq DESC via [] min match count = 0 (true), 1 not
    // success
    {
      std::map<ptrdiff_t,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, ['bca', 'def'], 0) "
          "SORT d.seq DESC "
          "RETURN d");
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

    // d.prefix = abc*|def*, d.seq DESC via [] min match count = 0 (true), 1
    // success
    {
      std::map<ptrdiff_t,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, ['abc', 'def'], 0) "
          "SORT d.seq DESC "
          "RETURN d");
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

    // d.prefix = abc*|def*, d.seq DESC via [] min match count = 1 (true)
    {
      std::map<ptrdiff_t,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const prefixSlice = docSlice.get("prefix");
        if (prefixSlice.isNone() ||
            !arangodb::iresearch::getStringRef(prefixSlice)
                 .starts_with("abc")) {
          continue;
        }
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, ['abc', 'def'], 1) "
          "SORT d.seq DESC "
          "RETURN d");
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

    // d.prefix = dfg*|def*, d.seq DESC via [] min match count = 1 (false)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, ['dfg', 'def'], 1) "
          "SORT d.seq DESC "
          "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.prefix = abc*|ab*, d.seq DESC via [] min match count = 2 (true)
    {
      std::map<ptrdiff_t,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const prefixSlice = docSlice.get("prefix");
        if (prefixSlice.isNone() ||
            !arangodb::iresearch::getStringRef(prefixSlice)
                 .starts_with("abc")) {
          continue;
        }
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, ['abc', 'ab'], 2) "
          "SORT "
          "d.seq DESC "
          "RETURN d");
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

    // d.prefix = abc*|def*, d.seq DESC via [] min match count = 2 (false)
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, ['abc', 'def'], 2) "
          "SORT d.seq DESC "
          "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.prefix = abc*|def*, d.seq DESC via [] min match count = 3 (false), 2
    // not success
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, ['abc', 'def'], 3) "
          "SORT d.seq DESC "
          "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // d.prefix = abc*|ab*, d.seq DESC via [] min match count = 3 (false), 2
    // success
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, ['abc', 'ab'], 3) "
          "SORT "
          "d.seq DESC "
          "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // Empty prefix - return all docs: d.prefix = ''*, TFIDF(), BM25(), d.seq
    // DESC
    {
      std::map<ptrdiff_t,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const prefixSlice = docSlice.get("prefix");
        if (prefixSlice.isNone()) {
          continue;
        }
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, '') SORT TFIDF(d), "
          "BM25(d), d.seq DESC RETURN d");
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

    // Empty prefix - return all docs: d.prefix = ''*, d.seq DESC via []
    {
      std::map<ptrdiff_t,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto const prefixSlice = docSlice.get("prefix");
        if (prefixSlice.isNone()) {
          continue;
        }
        expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH starts_with(d.prefix, ['', 'ab']) SORT "
          "d.seq DESC RETURN d");
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

    // invalid prefix
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH STARTS_WITH(d.prefix, "
          "'abc_invalid_prefix') "
          "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }

    // invalid prefix via []
    {
      auto queryResult = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH STARTS_WITH(d.prefix, "
          "['abc_invalid_prefix', "
          "'another_invalid_prefix']) "
          "RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
      EXPECT_FALSE(resultIt.valid());
    }
  }
};

class QueryStartsWithView : public QueryStartsWith {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }

  void createView() {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\"}");
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
            "version": $0 },
          "collection_2": {
            "version": $1,
            "includeAllFields": true }
      }})";

      auto viewDefinition = absl::Substitute(
          viewDefinitionTemplate, static_cast<uint32_t>(linkVersion()),
          static_cast<uint32_t>(linkVersion()));

      auto updateJson = arangodb::velocypack::Parser::fromJson(viewDefinition);
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

class QueryStartsWithSearch : public QueryStartsWith {
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
      collection->createIndex(createJson->slice(), created).get();
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

TEST_P(QueryStartsWithView, Test) {
  create();
  createView();
  populateData();
  queryTests();
}

TEST_P(QueryStartsWithSearch, Test) {
  create();
  createSearch();
  populateData();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryStartsWithView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryStartsWithSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
