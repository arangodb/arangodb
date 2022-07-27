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

class QueryInRange : public QueryTest {
 protected:
  void createCollections() {
    {
      auto createJson =
          velocypack::Parser::fromJson(R"({ "name": "testCollection0" })");
      auto collection = _vocbase.createCollection(createJson->slice());
      ASSERT_TRUE(collection);
      std::vector<std::shared_ptr<velocypack::Builder>> docs{
          velocypack::Parser::fromJson(R"({ "seq": -6, "value": null })"),
          velocypack::Parser::fromJson(R"({ "seq": -5, "value": true })"),
          velocypack::Parser::fromJson(R"({ "seq": -4, "value": "abc" })"),
          velocypack::Parser::fromJson(
              R"({ "seq": -3, "value": [ 3.14, -3.14 ] })"),
          velocypack::Parser::fromJson(
              R"({ "seq": -2, "value": [ 1, "abc" ] })"),
          velocypack::Parser::fromJson(
              R"({ "seq": -1, "value": { "a": 7, "b": "c" } })"),
      };

      OperationOptions options;
      options.returnNew = true;
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::Create(_vocbase), *collection,
          AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : docs) {
        auto r = trx.insert(collection->name(), entry->slice(), options);
        EXPECT_TRUE(r.ok());
        _insertedDocs.emplace_back(r.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }
    {
      auto createJson =
          velocypack::Parser::fromJson(R"({ "name": "testCollection1" })");
      auto collection = _vocbase.createCollection(createJson->slice());
      ASSERT_TRUE(collection);

      irs::utf8_path resource;
      resource /= std::string_view{testResourceDir};
      resource /= std::string_view{"simple_sequential.json"};

      auto builder =
          basics::VelocyPackHelper::velocyPackFromFile(resource.string());
      auto slice = builder.slice();
      ASSERT_TRUE(slice.isArray());

      OperationOptions options;
      options.returnNew = true;
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::Create(_vocbase), *collection,
          AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (velocypack::ArrayIterator it{slice}; it.valid(); ++it) {
        auto r = trx.insert(collection->name(), it.value(), options);
        EXPECT_TRUE(r.ok());
        _insertedDocs.emplace_back(r.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }
  }

  void queryTests() {
    // d.value > false && d.value <= true
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[1].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.value, false, true, false, "
            "true) "
            "SORT d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();

          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.value, false, true, "
            "false, "
            "true)) "
            "SORT d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.value >= null && d.value <= null
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[0].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.value, null, null, true, "
            "true) "
            "SORT d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.value, null, null, true, "
            "true)) "
            "SORT d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.value > null && d.value <= null
    {
      std::vector<velocypack::Slice> expected = {};
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.value, null, null, false, "
            "true) "
            "SORT d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.value, null, null, false, "
            "true)) "
            "SORT d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.name >= 'A' && d.name <= 'A'
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[6].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'A', true, true) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'A', 'A', true, "
            "true)) SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.name >= 'B' && d.name <= 'A'
    {
      std::vector<velocypack::Slice> expected = {};
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.name, 'B', 'A', true, true) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'B', 'A', true, "
            "true)) SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.name >= 'A' && d.name <= 'E'
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[6].slice(),  _insertedDocs[7].slice(),
          _insertedDocs[8].slice(),  _insertedDocs[9].slice(),
          _insertedDocs[10].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'E', true, true) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'A', 'E', true, "
            "true)) SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.name >= 'A' && d.name < 'E'
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[6].slice(),
          _insertedDocs[7].slice(),
          _insertedDocs[8].slice(),
          _insertedDocs[9].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'E', true, false) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'A', 'E', true, "
            "false)) SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.name > 'A' && d.name <= 'E'
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[7].slice(),
          _insertedDocs[8].slice(),
          _insertedDocs[9].slice(),
          _insertedDocs[10].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'E', false, true) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'A', 'E', false, "
            "true)) SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.name > 'A' && d.name < 'E'
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[7].slice(),
          _insertedDocs[8].slice(),
          _insertedDocs[9].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'E', false, false) "
            "SORT d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.name, 'A', 'E', false, "
            "false)) "
            "SORT d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.seq >= 5 && d.seq <= -1
    {
      std::vector<velocypack::Slice> expected = {};
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.seq, 5, -1, true, true) SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.seq, 5, -1, true, true)) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.seq >= 1 && d.seq <= 5
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[7].slice(),  _insertedDocs[8].slice(),
          _insertedDocs[9].slice(),  _insertedDocs[10].slice(),
          _insertedDocs[11].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.seq, 1, 5, true, true) SORT "
            "d.seq "
            "RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.seq, 1, 5, true, true)) "
            "SORT d.seq "
            "RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.seq > -2 && d.seq <= 5
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[5].slice(),  _insertedDocs[6].slice(),
          _insertedDocs[7].slice(),  _insertedDocs[8].slice(),
          _insertedDocs[9].slice(),  _insertedDocs[10].slice(),
          _insertedDocs[11].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.seq, -2, 5, false, true) SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.seq, -2, 5, false, true)) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.seq > 1 && d.seq < 5
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[8].slice(),
          _insertedDocs[9].slice(),
          _insertedDocs[10].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.seq, 1, 5, false, false) SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.seq, 1, 5, false, false)) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.seq >= 1 && d.seq < 5
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[7].slice(),
          _insertedDocs[8].slice(),
          _insertedDocs[9].slice(),
          _insertedDocs[10].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.seq, 1, 5, true, false) SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.seq, 1, 5, true, false)) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.value > 3 && d.value < 4
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[3].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.value, 3, 4, false, false) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.value, 3, 4, false, "
            "false)) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }

    // d.value > -4 && d.value < -3
    {
      std::vector<velocypack::Slice> expected = {
          _insertedDocs[3].slice(),
      };
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH IN_RANGE(d.value, -4, -3, false, false) "
            "SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;

        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_LT(i, expected.size());
          EXPECT_EQ(0, basics::VelocyPackHelper::compare(expected[i++],
                                                         resolved, true));
        }
        EXPECT_EQ(i, expected.size());
      }
      // NOT
      {
        auto result = executeQuery(
            _vocbase,
            "FOR d IN testView SEARCH NOT(IN_RANGE(d.value, -4, -3, false, "
            "false)) SORT "
            "d.seq RETURN d");
        ASSERT_TRUE(result.result.ok());
        auto slice = result.data->slice();
        EXPECT_TRUE(slice.isArray());
        size_t i = 0;
        for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
          auto const resolved = itr.value().resolveExternals();
          for (const auto& u : expected) {
            EXPECT_NE(0, basics::VelocyPackHelper::compare(u, resolved, true));
          }
          ++i;
        }
        EXPECT_EQ(i, (_insertedDocs.size() - expected.size()));
      }
    }
  }
};

class QueryInRangeView : public QueryInRange {
 protected:
  ViewType type() const final { return ViewType::kView; }
};

class QueryInRangeSearch : public QueryInRange {
 protected:
  ViewType type() const final { return ViewType::kSearch; }
};

TEST_P(QueryInRangeView, Test) {
  createCollections();
  createView(R"("analyzers": [ "test_analyzer", "identity" ],
                "trackListPositions": false,
                "storeValues": "id",)",
             R"("analyzers": [ "test_analyzer", "identity" ],
                "storeValues": "id",)");
  queryTests();
}

TEST_P(QueryInRangeSearch, TestTestAnalyzer) {
  createCollections();
  createIndexes(R"("analyzer": "test_analyzer",
                   "trackListPositions": false,
                   "storeValues": "id",)",
                R"("analyzer": "test_analyzer",
                   "storeValues": "id",)");
  createSearch();
  queryTests();
}

TEST_P(QueryInRangeSearch, TestIdentity) {
  createCollections();
  createIndexes(R"("analyzer": "identity",
                   "trackListPositions": false,
                   "storeValues": "id",)",
                R"("analyzer": "identity",
                   "storeValues": "id",)");
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryInRangeView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryInRangeSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
