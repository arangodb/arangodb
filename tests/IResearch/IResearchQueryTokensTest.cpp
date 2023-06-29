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
class TestDelimAnalyzer final
    : public irs::analysis::TypedAnalyzer<TestDelimAnalyzer> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "TestDelimAnalyzer";
  }

  static ptr make(std::string_view args) {
    auto slice = arangodb::iresearch::slice(args);
    if (slice.isNull()) throw std::exception();
    if (slice.isNone()) return nullptr;
    if (slice.isString()) {
      return std::make_unique<TestDelimAnalyzer>(
          arangodb::iresearch::getStringRef(slice));
    } else if (slice.isObject() && slice.hasKey("args") &&
               slice.get("args").isString()) {
      return std::make_unique<TestDelimAnalyzer>(
          arangodb::iresearch::getStringRef(slice.get("args")));
    } else {
      return nullptr;
    }
  }

  static bool normalize(std::string_view args, std::string& out) {
    auto slice = arangodb::iresearch::slice(args);
    if (slice.isNull()) throw std::exception();
    if (slice.isNone()) return false;
    arangodb::velocypack::Builder builder;
    if (slice.isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(
          builder, "args", arangodb::iresearch::getStringRef(slice));
    } else if (slice.isObject() && slice.hasKey("args") &&
               slice.get("args").isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(
          builder, "args",
          arangodb::iresearch::getStringRef(slice.get("args")));
    } else {
      return false;
    }

    out = builder.buffer()->toString();
    return true;
  }

  TestDelimAnalyzer(std::string_view delim)
      : _delim(irs::ViewCast<irs::byte_type>(delim)) {}

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    if (type == irs::type<irs::term_attribute>::id()) {
      return &_term;
    }
    return nullptr;
  }

  bool next() final {
    if (_data.empty()) {
      return false;
    }

    size_t i = 0;

    for (size_t count = _data.size(); i < count; ++i) {
      auto data = irs::ViewCast<char>(_data);
      auto delim = irs::ViewCast<char>(irs::bytes_view{_delim});

      if (0 == strncmp(&(data.data()[i]), delim.data(), delim.size())) {
        _term.value = irs::bytes_view(_data.data(), i);
        _data = irs::bytes_view(
            _data.data() + i + (std::max)(size_t(1), _delim.size()),
            _data.size() - i - (std::max)(size_t(1), _delim.size()));
        return true;
      }
    }

    _term.value = _data;
    _data = irs::bytes_view{};
    return true;
  }

  bool reset(std::string_view data) final {
    _data = irs::ViewCast<irs::byte_type>(data);
    return true;
  }

 private:
  irs::bstring _delim;
  irs::bytes_view _data;
  irs::term_attribute _term;
};

REGISTER_ANALYZER_VPACK(TestDelimAnalyzer, TestDelimAnalyzer::make,
                        TestDelimAnalyzer::normalize);

class QueryTokens : public QueryTest {
 protected:
  void queryTests() {
    // test no-match
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.prefix IN TOKENS('def', "
          "'test_csv_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
          "d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test no-match via []
    {
      std::vector<arangodb::velocypack::Slice> expected = {};
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d['prefix'] IN TOKENS('def', "
          "'test_csv_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
          "d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test single match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[9].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.prefix IN TOKENS('ab,abcde,de', "
          "'test_csv_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
          "d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;
      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test single match via []
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[9].slice(),
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d['prefix'] IN TOKENS('ab,abcde,de', "
          "'test_csv_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
          "d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test mulptiple match
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[36].slice(),  // (duplicate term)
          _insertedDocs[37].slice(),  // (duplicate term)
          _insertedDocs[6].slice(),   // (unique term)
          _insertedDocs[26].slice(),  // (unique term)
      };
      auto result = arangodb::tests::executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.prefix IN TOKENS('z,xy,abcy,abcd,abc', "
          "'test_csv_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN "
          "d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }

    // test mulptiple match via []
    {
      std::vector<arangodb::velocypack::Slice> expected = {
          _insertedDocs[36].slice(),  // (duplicate term)
          _insertedDocs[37].slice(),  // (duplicate term)
          _insertedDocs[6].slice(),   // (unique term)
          _insertedDocs[26].slice(),  // (unique term)
      };
      auto result =
          arangodb::tests::executeQuery(_vocbase,
                                        "FOR d IN testView SEARCH d['prefix'] "
                                        "IN TOKENS('z,xy,abcy,abcd,abc', "
                                        "'test_csv_analyzer') SORT BM25(d) "
                                        "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                              expected[i++], resolved, true)));
      }

      EXPECT_EQ(i, expected.size());
    }
  }
};

class QueryTokensView : public QueryTokens {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }
};

class QueryTokensSearch : public QueryTokens {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }
};

TEST_P(QueryTokensView, Test) {
  createCollections();
  createView(R"("trackListPositions": true,)", R"()");
  queryTests();
}

TEST_P(QueryTokensSearch, Test) {
  createCollections();
  createIndexes(R"("trackListPositions": true,)", R"()");
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryTokensView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryTokensSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
