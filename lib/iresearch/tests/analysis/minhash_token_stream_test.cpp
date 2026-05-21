////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

#include "analysis/minhash_token_stream.hpp"

#include "analysis/analyzers.hpp"
#include "analysis/segmentation_token_stream.hpp"
#include "analysis/token_streams.hpp"
#include "tests_shared.hpp"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

namespace {

class ArrayStream final : public irs::analysis::TypedAnalyzer<ArrayStream> {
 public:
  ArrayStream(std::string_view data, const std::string_view* begin,
              const std::string_view* end) noexcept
    : data_{data}, begin_{begin}, it_{end}, end_{end} {}

  bool next() final {
    if (it_ == end_) {
      return false;
    }

    auto& offs = std::get<irs::offset>(attrs_);
    offs.start = offs.end;
    offs.end += it_->size();

    std::get<irs::term_attribute>(attrs_).value =
      irs::ViewCast<irs::byte_type>(*it_);

    ++it_;
    return true;
  }

  irs::attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    return irs::get_mutable(attrs_, id);
  }

  bool reset(std::string_view data) final {
    std::get<irs::offset>(attrs_) = {};

    if (data == data_) {
      it_ = begin_;
      return true;
    }

    it_ = end_;
    return false;
  }

 private:
  using attributes =
    std::tuple<irs::term_attribute, irs::increment, irs::offset>;

  attributes attrs_;
  std::string_view data_;
  const std::string_view* begin_;
  const std::string_view* it_;
  const std::string_view* end_;
};

}  // namespace

TEST(MinHashTokenStreamTest, CheckConsts) {
  static_assert("minhash" ==
                irs::type<irs::analysis::MinHashTokenStream>::name());
}

TEST(MinHashTokenStreamTest, NormalizeDefault) {
  std::string out;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({"numHashes": 42})"));
  const auto expected_out =
    arangodb::velocypack::Parser::fromJson(R"({"numHashes": 42})");
  ASSERT_EQ(expected_out->slice().toString(), out);

  // Failing cases
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({"numHashes": "42"})"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({"numHashes": null})"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({"numHashes": 0})"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({"numHashes": false})"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({"numHashes": []})"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({"numHashes": {}})"));
}

TEST(MinHashTokenStreamTest, ConstructDefault) {
  auto assert_analyzer = [](const irs::analysis::analyzer::ptr& stream,
                            size_t expected_num_hashes) {
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::MinHashTokenStream>::id(),
              stream->type());

    auto* impl =
      dynamic_cast<const irs::analysis::MinHashTokenStream*>(stream.get());
    ASSERT_NE(nullptr, impl);
    const auto& [analyzer, num_hashes] = impl->options();
    ASSERT_NE(nullptr, analyzer);
    ASSERT_EQ(irs::type<irs::string_token_stream>::id(), analyzer->type());
    ASSERT_EQ(expected_num_hashes, num_hashes);
  };

  assert_analyzer(irs::analysis::analyzers::get(
                    "minhash", irs::type<irs::text_format::json>::get(),
                    R"({"numHashes": 42})"),
                  42);

  // Failing cases
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"numHashes": 0})"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"numHashes": []})"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"numHashes": {}})"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"numHashes": true})"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"numHashes": null})"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"numHashes": "42"})"));
  ASSERT_EQ(nullptr,
            irs::analysis::analyzers::get(
              "minhash", irs::type<irs::text_format::json>::get(), R"({})"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"analyzer":{}})"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"analyzer":""})"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"analyzer":null})"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"analyzer":[]})"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                       "minhash", irs::type<irs::text_format::json>::get(),
                       R"({"analyzer":42})"));
}

TEST(MinHashTokenStreamTest, ConstructCustom) {
  auto assert_analyzer = [](const irs::analysis::analyzer::ptr& stream,
                            size_t expected_num_hashes) {
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::MinHashTokenStream>::id(),
              stream->type());

    auto* impl =
      dynamic_cast<const irs::analysis::MinHashTokenStream*>(stream.get());
    ASSERT_NE(nullptr, impl);
    const auto& [analyzer, num_hashes] = impl->options();
    ASSERT_EQ(expected_num_hashes, num_hashes);
    ASSERT_NE(nullptr, analyzer);
    ASSERT_EQ(irs::type<irs::analysis::segmentation_token_stream>::id(),
              analyzer->type());
  };

  assert_analyzer(
    irs::analysis::analyzers::get(
      "minhash", irs::type<irs::text_format::json>::get(),
      R"({ "analyzer":{"type":"segmentation"}, "numHashes": 42 })"),
    42);
}

TEST(MinHashTokenStreamTest, NormalizeCustom) {
  std::string out;
  const auto expected_out = arangodb::velocypack::Parser::fromJson(
    R"({ "analyzer": {
             "type":"segmentation",
             "properties": {"break":"alpha","case":"lower"} },
             "numHashes": 42 })");

  out.clear();
  ASSERT_TRUE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({ "analyzer":{"type":"segmentation"}, "numHashes": 42 })"));
  ASSERT_EQ(expected_out->slice().toString(), out);

  out.clear();
  ASSERT_TRUE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({ "analyzer":{"type":"segmentation", "properties":{}}, "numHashes": 42 })"));
  ASSERT_EQ(expected_out->slice().toString(), out);

  out.clear();
  ASSERT_TRUE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({ "analyzer":{"type":"segmentation", "properties":{"case":"lower"}}, "numHashes": 42 })"));
  ASSERT_EQ(expected_out->slice().toString(), out);

  out.clear();
  ASSERT_TRUE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({ "analyzer":{"type":"segmentation", "properties":{"case":"upper"}}, "numHashes": 42 })"));
  ASSERT_NE(expected_out->slice().toString(), out);

  // Failing cases
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({ "analyzer":{"type":"segmentation", "properties":{}, "numHashes": 0 })"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({ "analyzer":{"type":"segmentation", "properties":false, "numHashes": 42 })"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({ "analyzer":{"type":"segmentation", "properties":[], "numHashes": 42 })"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({ "analyzer":{"type":"segmentation", "properties":false, "numHashes": 42 })"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({ "analyzer":{"type":"segmentation", "properties":null, "numHashes": 42 })"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(
    out, "minhash", irs::type<irs::text_format::json>::get(),
    R"({ "analyzer":{"type":"segmentation", "properties":42, "numHashes": 42 })"));
}

TEST(MinHashTokenStreamTest, CheckOptions) {
  using namespace irs::analysis;

  MinHashTokenStream::Options opts;

  ASSERT_EQ(nullptr, opts.analyzer);
  ASSERT_EQ(1, opts.num_hashes);
}

TEST(MinHashTokenStreamTest, ConstructFromOptions) {
  using namespace irs::analysis;

  {
    MinHashTokenStream stream{
      MinHashTokenStream::Options{.analyzer = nullptr, .num_hashes = 0}};
    ASSERT_NE(nullptr, irs::get<irs::term_attribute>(stream));
    ASSERT_NE(nullptr, irs::get<irs::offset>(stream));
    ASSERT_NE(nullptr, irs::get<irs::increment>(stream));
    const auto& [analyzer, num_hashes] = stream.options();
    ASSERT_NE(nullptr, analyzer);
    ASSERT_EQ(0, num_hashes);
    ASSERT_EQ(irs::type<irs::string_token_stream>::id(), analyzer->type());
  }

  {
    MinHashTokenStream stream{MinHashTokenStream::Options{
      .analyzer = std::make_unique<segmentation_token_stream>(
        segmentation_token_stream::options_t{}),
      .num_hashes = 42}};
    ASSERT_NE(nullptr, irs::get<irs::term_attribute>(stream));
    ASSERT_NE(nullptr, irs::get<irs::offset>(stream));
    ASSERT_NE(nullptr, irs::get<irs::increment>(stream));
    const auto& [analyzer, num_hashes] = stream.options();
    ASSERT_NE(nullptr, analyzer);
    ASSERT_EQ(42, num_hashes);
    ASSERT_EQ(irs::type<segmentation_token_stream>::id(), analyzer->type());
  }

  {
    MinHashTokenStream stream{MinHashTokenStream::Options{
      .analyzer = std::make_unique<empty_analyzer>(), .num_hashes = 42}};
    ASSERT_NE(nullptr, irs::get<irs::term_attribute>(stream));
    ASSERT_NE(nullptr, irs::get<irs::offset>(stream));
    ASSERT_NE(nullptr, irs::get<irs::increment>(stream));
    const auto& [analyzer, num_hashes] = stream.options();
    ASSERT_NE(nullptr, analyzer);
    ASSERT_EQ(42, num_hashes);
    ASSERT_EQ(irs::type<empty_analyzer>::id(), analyzer->type());
    ASSERT_FALSE(stream.reset(""));
  }
}

TEST(MinHashTokenStreamTest, NextReset) {
  using namespace irs::analysis;

  constexpr uint32_t kNumHashes = 4;
  constexpr std::string_view kData{"Hund"};
  constexpr std::string_view kValues[]{"quick", "brown", "fox",  "jumps",
                                       "over",  "the",   "lazy", "dog"};

  MinHashTokenStream stream{{.analyzer = std::make_unique<ArrayStream>(
                               kData, std::begin(kValues), std::end(kValues)),
                             .num_hashes = kNumHashes}};

  auto* term = irs::get<irs::term_attribute>(stream);
  ASSERT_NE(nullptr, term);
  auto* offset = irs::get<irs::offset>(stream);
  ASSERT_NE(nullptr, offset);
  auto* inc = irs::get<irs::increment>(stream);
  ASSERT_NE(nullptr, inc);

  for (size_t i = 0; i < 2; ++i) {
    ASSERT_TRUE(stream.reset(kData));

    ASSERT_TRUE(stream.next());
    EXPECT_EQ("aq+fPy7QMmE", irs::ViewCast<char>(term->value));
    ASSERT_EQ(1, inc->value);
    EXPECT_EQ(0, offset->start);
    EXPECT_EQ(32, offset->end);

    ASSERT_TRUE(stream.next());
    EXPECT_EQ("zN55OxHobU0", irs::ViewCast<char>(term->value));
    ASSERT_EQ(0, inc->value);
    EXPECT_EQ(0, offset->start);
    EXPECT_EQ(32, offset->end);

    ASSERT_TRUE(stream.next());
    EXPECT_EQ("fE1vyfdbgBg", irs::ViewCast<char>(term->value));
    ASSERT_EQ(0, inc->value);
    EXPECT_EQ(0, offset->start);
    EXPECT_EQ(32, offset->end);

    ASSERT_TRUE(stream.next());
    EXPECT_EQ("g44ma/eW5Rc", irs::ViewCast<char>(term->value));
    ASSERT_EQ(0, inc->value);
    EXPECT_EQ(0, offset->start);
    EXPECT_EQ(32, offset->end);

    ASSERT_FALSE(stream.next());
  }
}
