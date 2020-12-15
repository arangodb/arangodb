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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include "gtest/gtest.h"
#include "tests_config.hpp"

#include "analysis/pipeline_token_stream.hpp"
#include "analysis/text_token_stream.hpp"
#include "analysis/ngram_token_stream.hpp"
#include "analysis/delimited_token_stream.hpp"
#include "analysis/text_token_stream.hpp"

#include "analysis/token_attributes.hpp"
#include "analysis/token_stream.hpp"
#include "utils/locale_utils.hpp"
#include "utils/runtime_utils.hpp"
#include "utils/utf8_path.hpp"

#include <rapidjson/document.h> // for rapidjson::Document, rapidjson::Value

#ifndef IRESEARCH_DLL
namespace {

class pipeline_test_analyzer : public irs::frozen_attributes<4, irs::analysis::analyzer>, private irs::util::noncopyable {
 public:
  pipeline_test_analyzer(bool has_offset, irs::bytes_ref payload)
    : attributes{ {
        { irs::type<irs::payload>::id(), payload.null() ? nullptr : &payload_},
        { irs::type<irs::increment>::id(), &inc_},
        { irs::type<irs::offset>::id(), has_offset? &offs_: nullptr},
        { irs::type<irs::term_attribute>::id(), &term_}},
        irs::type<pipeline_test_analyzer>::get()} {
    payload_.value = payload;
  }
  virtual bool next() override {
    if (term_emitted) {
      return false;
    }
    term_emitted = false;
    inc_.value = 1;
    offs_.start = 0;
    offs_.end = static_cast<uint32_t>(term_.value.size());
    return true;
  }
  virtual bool reset(const irs::string_ref& data) override {
    term_emitted = false;
    term_.value = irs::ref_cast<irs::byte_type>(data);
    return true;
  }

 private:
  bool term_emitted{ true };
  irs::term_attribute term_;
  irs::payload payload_;
  irs::offset offs_;
  irs::increment inc_;
};

class pipeline_test_analyzer2 : public irs::frozen_attributes<3, irs::analysis::analyzer>, private irs::util::noncopyable {
 public:
  pipeline_test_analyzer2(std::vector<std::pair<uint32_t, uint32_t>>&& offsets,
    std::vector<uint32_t>&& increments,
    std::vector<bool>&& nexts, std::vector<bool>&& resets,
    std::vector<irs::bytes_ref>&& terms)
    : attributes{ {
      { irs::type<irs::increment>::id(), &inc_},
      { irs::type<irs::offset>::id(), &offs_},
      { irs::type<irs::term_attribute>::id(), &term_}},
      irs::type<pipeline_test_analyzer>::get() },
      offsets_(offsets), increments_(increments), nexts_(nexts), resets_(resets),
      terms_(terms) {
    current_offset_ = offsets_.begin();
    current_increment_ = increments_.begin();
    current_next_ = nexts_.begin();
    current_reset_ = resets_.begin();
    current_term_ = terms_.begin();
  }
  virtual bool next() override {
    if (current_next_ != nexts_.end()) {
      auto next_val = *(current_next_++);
      if (next_val) {
        if (current_offset_ != offsets_.end()) {
          auto value = *(current_offset_++);
          offs_.start = value.first;
          offs_.end = value.second;
        } else {
          offs_.start = 0;
          offs_.end = 0;
        }
        if (current_increment_ != increments_.end()) {
          inc_.value = *(current_increment_++);
        } else {
          inc_.value = 0;
        }
        if (current_term_ != terms_.end()) {
          term_.value = *(current_term_++);
        } else {
          term_.value = irs::bytes_ref::NIL;
        }
      }
      return next_val;
    }
    return false;
  }
  virtual bool reset(const irs::string_ref& data) override {
    if (current_reset_ != resets_.end()) {
      return *(current_reset_++);
    }
    return false;
  }

 private:
  std::vector<std::pair<uint32_t, uint32_t>> offsets_;
  std::vector<std::pair<uint32_t, uint32_t>>::const_iterator current_offset_;
  std::vector<uint32_t> increments_;
  std::vector<uint32_t>::const_iterator current_increment_;
  std::vector<bool> nexts_;
  std::vector<bool>::const_iterator current_next_;
  std::vector<bool> resets_;
  std::vector<bool>::const_iterator current_reset_;
  std::vector<irs::bytes_ref> terms_;
  std::vector<irs::bytes_ref>::const_iterator current_term_;

  irs::term_attribute term_;
  irs::offset offs_;
  irs::increment inc_;
};


struct analyzer_token {
  irs::string_ref value;
  size_t start;
  size_t end;
  uint32_t pos;
};

using analyzer_tokens = std::vector<analyzer_token>;


void assert_pipeline(irs::analysis::analyzer* pipe, const std::string& data, const analyzer_tokens& expected_tokens) {
  SCOPED_TRACE(data);
  auto* offset = irs::get<irs::offset>(*pipe);
  ASSERT_TRUE(offset);
  auto* term = irs::get<irs::term_attribute>(*pipe);
  ASSERT_TRUE(term);
  auto* inc = irs::get<irs::increment>(*pipe);
  ASSERT_TRUE(inc);
  ASSERT_TRUE(pipe->reset(data));
  uint32_t pos{ irs::integer_traits<uint32_t>::const_max };
  auto expected_token = expected_tokens.begin();
  while (pipe->next()) {
    auto term_value = std::string(irs::ref_cast<char>(term->value).c_str(), term->value.size());
    SCOPED_TRACE(testing::Message("Term:") << term_value);
    auto old_pos = pos;
    pos += inc->value;
    ASSERT_NE(expected_token, expected_tokens.end());
    ASSERT_EQ(irs::ref_cast<irs::byte_type>(expected_token->value), term->value);
    ASSERT_EQ(expected_token->start, offset->start);
    ASSERT_EQ(expected_token->end, offset->end);
    ASSERT_EQ(expected_token->pos, pos);
    ++expected_token;
  }
  ASSERT_EQ(expected_token, expected_tokens.end());
}

}

TEST(pipeline_token_stream_test, consts) {
  static_assert("pipeline" == irs::type<irs::analysis::pipeline_token_stream>::name());
}

TEST(pipeline_token_stream_test, empty_pipeline) {
  irs::analysis::pipeline_token_stream::options_t pipeline_options;
  irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));

  std::string data = "quick broWn,, FOX  jumps,  over lazy dog";
  ASSERT_FALSE(pipe.reset(data));
}

TEST(pipeline_token_stream_test, many_tokenizers) {
  auto delimiter = irs::analysis::analyzers::get("delimiter",
    irs::type<irs::text_format::json>::get(),
    "{\"delimiter\":\",\"}");

  auto delimiter2 = irs::analysis::analyzers::get("delimiter",
    irs::type<irs::text_format::json>::get(),
    "{\"delimiter\":\" \"}");

  auto text = irs::analysis::analyzers::get("text",
    irs::type<irs::text_format::json>::get(),
    "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[], \"case\":\"none\", \"stemming\":false }");

  auto ngram = irs::analysis::analyzers::get("ngram",
    irs::type<irs::text_format::json>::get(),
    "{\"min\":2, \"max\":2, \"preserveOriginal\":true }");

  irs::analysis::pipeline_token_stream::options_t pipeline_options;
  pipeline_options.push_back(delimiter);
  pipeline_options.push_back(delimiter2);
  pipeline_options.push_back(text);
  pipeline_options.push_back(ngram);

  irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
  ASSERT_EQ(irs::type<irs::analysis::pipeline_token_stream>::id(), pipe.type());

  std::string data = "quick broWn,, FOX  jumps,  over lazy dog";
  const analyzer_tokens expected{
    { "qu", 0, 2, 0},
    { "quick", 0, 5, 0},
    { "ui", 1, 3, 1},
    { "ic", 2, 4, 2},
    { "ck", 3, 5, 3},
    { "br", 6, 8, 4},
    { "broWn", 6, 11, 4},
    { "ro", 7, 9, 5},
    { "oW", 8, 10, 6},
    { "Wn", 9, 11, 7},
    { "FO", 14, 16, 8},
    { "FOX", 14, 17, 8},
    { "OX", 15, 17, 9},
    { "ju", 19, 21, 10},
    { "jumps", 19, 24, 10},
    { "um", 20, 22, 11},
    { "mp", 21, 23, 12},
    { "ps", 22, 24, 13},
    { "ov", 27, 29, 14},
    { "over", 27, 31, 14},
    { "ve", 28, 30, 15},
    { "er", 29, 31, 16},
    { "la", 32, 34, 17},
    { "lazy", 32, 36, 17},
    { "az", 33, 35, 18},
    { "zy", 34, 36, 19},
    { "do", 37, 39, 20},
    { "dog", 37, 40, 20},
    { "og", 38, 40, 21},
  };
  assert_pipeline(&pipe, data, expected);
}

TEST(pipeline_token_stream_test, overlapping_ngrams) {
  auto ngram = irs::analysis::analyzers::get("ngram",
    irs::type<irs::text_format::json>::get(),
    "{\"min\":6, \"max\":7, \"preserveOriginal\":false }");
  auto ngram2 = irs::analysis::analyzers::get("ngram",
    irs::type<irs::text_format::json>::get(),
    "{\"min\":2, \"max\":3, \"preserveOriginal\":false }");

  irs::analysis::pipeline_token_stream::options_t pipeline_options;
  pipeline_options.push_back(ngram);
  pipeline_options.push_back(ngram2);
  irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));

  std::string data = "ABCDEFJH";
  const analyzer_tokens expected{
    {"AB", 0, 2, 0}, {"ABC", 0, 3, 0}, {"BC", 1, 3, 1}, {"BCD", 1, 4, 1},
    {"CD", 2, 4, 2}, {"CDE", 2, 5, 2}, {"DE", 3, 5, 3}, {"DEF", 3, 6, 3},
    {"EF", 4, 6, 4}, {"AB", 0, 2, 5}, {"ABC", 0, 3, 5}, {"BC", 1, 3, 6},
    {"BCD", 1, 4, 6}, {"CD", 2, 4, 7}, {"CDE", 2, 5, 7}, {"DE", 3, 5, 8},
    {"DEF", 3, 6, 8}, {"EF", 4, 6, 9}, {"EFJ", 4, 7, 9}, {"FJ", 5, 7, 10},
    {"BC", 1, 3, 11}, {"BCD", 1, 4, 11}, {"CD", 2, 4, 12}, {"CDE", 2, 5, 12},
    {"DE", 3, 5, 13}, {"DEF", 3, 6, 13}, {"EF", 4, 6, 14}, {"EFJ", 4, 7, 14},
    {"FJ", 5, 7, 15}, {"BC", 1, 3, 16}, {"BCD", 1, 4, 16}, {"CD", 2, 4, 17},
    {"CDE", 2, 5, 17}, {"DE", 3, 5, 18}, {"DEF", 3, 6, 18}, {"EF", 4, 6, 19},
    {"EFJ", 4, 7, 19}, {"FJ", 5, 7, 20}, {"FJH", 5, 8, 20}, {"JH", 6, 8, 21},
    {"CD", 2, 4, 22}, {"CDE", 2, 5, 22}, {"DE", 3, 5, 23}, {"DEF", 3, 6, 23},
    {"EF", 4, 6, 24}, {"EFJ", 4, 7, 24}, {"FJ", 5, 7, 25}, {"FJH", 5, 8, 25}, {"JH", 6, 8, 26},
  };
  assert_pipeline(&pipe, data, expected);
}


TEST(pipeline_token_stream_test, case_ngrams) {
  auto ngram = irs::analysis::analyzers::get("ngram",
    irs::type<irs::text_format::json>::get(),
    "{\"min\":3, \"max\":3, \"preserveOriginal\":false }");
  auto norm = irs::analysis::analyzers::get("norm",
    irs::type<irs::text_format::json>::get(),
    "{\"locale\":\"en\", \"case\":\"upper\"}");
  std::string data = "QuIck BroWN FoX";
  const analyzer_tokens expected{
    {"QUI", 0, 3, 0}, {"UIC", 1, 4, 1}, {"ICK", 2, 5, 2},
    {"CK ", 3, 6, 3}, {"K B", 4, 7, 4}, {" BR", 5, 8, 5},
    {"BRO", 6, 9, 6}, {"ROW", 7, 10, 7}, {"OWN", 8, 11, 8},
    {"WN ", 9, 12, 9}, {"N F", 10, 13, 10}, {" FO", 11, 14, 11},
    {"FOX", 12, 15, 12},
  };
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.push_back(ngram);
    pipeline_options.push_back(norm);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    assert_pipeline(&pipe, data, expected);
  }
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.push_back(norm);
    pipeline_options.push_back(ngram);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    assert_pipeline(&pipe, data, expected);
  }
}

TEST(pipeline_token_stream_test, no_tokenizers) {
  auto norm1 = irs::analysis::analyzers::get("norm",
    irs::type<irs::text_format::json>::get(),
    "{\"locale\":\"en\", \"case\":\"upper\"}");
  auto norm2 = irs::analysis::analyzers::get("norm",
    irs::type<irs::text_format::json>::get(),
    "{\"locale\":\"en\", \"case\":\"lower\"}");
  std::string data = "QuIck";
  const analyzer_tokens expected{
    {"quick", 0, 5, 0},
  };
  irs::analysis::pipeline_token_stream::options_t pipeline_options;
  pipeline_options.push_back(norm1);
  pipeline_options.push_back(norm2);
  irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
  assert_pipeline(&pipe, data, expected);
}

TEST(pipeline_token_stream_test, source_modification_tokenizer) {
  auto text = irs::analysis::analyzers::get("text",
    irs::type<irs::text_format::json>::get(),
    "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[], \"case\":\"none\", \"stemming\":true }");
  auto norm = irs::analysis::analyzers::get("norm",
    irs::type<irs::text_format::json>::get(),
    "{\"locale\":\"en\", \"case\":\"lower\"}");
  std::string data = "QuIck broWn fox jumps";
  const analyzer_tokens expected{
    {"quick", 0, 5, 0},
    {"brown", 6, 11, 1},
    {"fox", 12, 15, 2},
    {"jump", 16, 21, 3}
  };
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.push_back(text);
    pipeline_options.push_back(norm);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    assert_pipeline(&pipe, data, expected);
  }
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.push_back(norm);
    pipeline_options.push_back(text);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    assert_pipeline(&pipe, data, expected);
  }
}

TEST(pipeline_token_stream_test, signle_tokenizer) {
  auto text = irs::analysis::analyzers::get("text",
    irs::type<irs::text_format::json>::get(),
    "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[], \"case\":\"lower\", \"stemming\":true }");
  std::string data = "QuIck broWn fox jumps";
  const analyzer_tokens expected{
    {"quick", 0, 5, 0},
    {"brown", 6, 11, 1},
    {"fox", 12, 15, 2},
    {"jump", 16, 21, 3}
  };
  irs::analysis::pipeline_token_stream::options_t pipeline_options;
  pipeline_options.push_back(text);
  irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
  assert_pipeline(&pipe, data, expected);
}

TEST(pipeline_token_stream_test, signle_non_tokenizer) {
  auto norm = irs::analysis::analyzers::get("norm",
    irs::type<irs::text_format::json>::get(),
    "{\"locale\":\"en\", \"case\":\"lower\"}");
  std::string data = "QuIck";
  const analyzer_tokens expected{
    {"quick", 0, 5, 0}
  };
  irs::analysis::pipeline_token_stream::options_t pipeline_options;
  pipeline_options.push_back(norm);
  irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
  assert_pipeline(&pipe, data, expected);
}

TEST(pipeline_token_stream_test, hold_position_tokenizer) {
  auto ngram = irs::analysis::analyzers::get("ngram",
    irs::type<irs::text_format::json>::get(),
    "{\"min\":2, \"max\":3, \"preserveOriginal\":true }");
  auto norm = irs::analysis::analyzers::get("norm",
    irs::type<irs::text_format::json>::get(),
    "{\"locale\":\"en\", \"case\":\"lower\"}");
  std::string data = "QuIck";
  const analyzer_tokens expected{
    {"qu", 0, 2, 0},
    {"qui", 0, 3, 0},
    {"quick", 0, 5, 0},
    {"ui", 1, 3, 1},
    {"uic", 1, 4, 1},
    {"ic", 2, 4, 2},
    {"ick", 2, 5, 2},
    {"ck", 3, 5, 3},
  };
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.push_back(ngram);
    pipeline_options.push_back(norm);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    assert_pipeline(&pipe, data, expected);
  }
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.push_back(norm);
    pipeline_options.push_back(ngram);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    assert_pipeline(&pipe, data, expected);
  }
}

TEST(pipeline_token_stream_test, hold_position_tokenizer2) {
  std::string data = "A";
  irs::bytes_ref term = irs::ref_cast<irs::byte_type>(irs::string_ref(data));
  irs::analysis::analyzer::ptr tokenizer1;
  {
    std::vector<std::pair<uint32_t, uint32_t>> offsets{ {0, 5}, { 0, 5 }};
    std::vector<uint32_t> increments{1, 0 };
    std::vector<bool> nexts{ true, true };
    std::vector<bool> resets{ true };
    std::vector<irs::bytes_ref> terms{ term };
    tokenizer1.reset(
       new pipeline_test_analyzer2(std::move(offsets),
                                   std::move(increments),
                                   std::move(nexts), std::move(resets),
                                   std::move(terms)));
  }
  irs::analysis::analyzer::ptr tokenizer2;
  {
    std::vector<std::pair<uint32_t, uint32_t>> offsets{ {0, 5}, { 1, 5 }, {2, 5}, { 2, 5 }};
    std::vector<uint32_t> increments{ 1, 1, 1, 0 };
    std::vector<bool> nexts{ true, true, false, true, true };
    std::vector<bool> resets{ true, true };
    std::vector<irs::bytes_ref> terms{ term };
    tokenizer2.reset(
      new pipeline_test_analyzer2(std::move(offsets),
        std::move(increments),
        std::move(nexts), std::move(resets),
        std::move(terms)));
  }
  irs::analysis::analyzer::ptr tokenizer3;
  {
    std::vector<std::pair<uint32_t, uint32_t>> offsets{ {0, 1}, { 0, 1 }};
    std::vector<uint32_t> increments{ 1, 1 };
    std::vector<bool> nexts{ true, false, false, false, true };
    std::vector<bool> resets{ true, true, true, true };
    std::vector<irs::bytes_ref> terms{ term, term };
    tokenizer3.reset(
      new pipeline_test_analyzer2(std::move(offsets),
        std::move(increments),
        std::move(nexts), std::move(resets),
        std::move(terms)));
  }

  const analyzer_tokens expected{
    {data, 0, 5, 0},
    {data, 2, 3, 1}
  };
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.push_back(tokenizer1);
    pipeline_options.push_back(tokenizer2);
    pipeline_options.push_back(tokenizer3);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    assert_pipeline(&pipe, data, expected);
  }
}


TEST(pipeline_token_stream_test, test_construct) {
  std::string config = "{\"pipeline\":["
                           "{\"type\":\"delimiter\", \"properties\": {\"delimiter\":\"A\"}},"
                           "{\"type\":\"text\", \"properties\":{\"locale\":\"en_US.UTF-8\",\"case\":\"lower\","
                             "\"accent\":false,\"stemming\":true,\"stopwords\":[\"fox\"]}},"
                           "{\"type\":\"norm\", \"properties\": {\"locale\":\"en_US.UTF-8\", \"case\":\"upper\"}}"
                        "]}";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_NE(nullptr, stream);
  const analyzer_tokens expected{
    {"QUICK", 0, 5, 0},
    {"BROWN", 6, 11, 1},
    {"JUMP", 16, 21, 2}
  };
  assert_pipeline(stream.get(), "QuickABrownAFOXAjUmps", expected);
}

TEST(pipeline_token_stream_test, test_normalized_construct) {
  std::string config = "{\"pipeline\":["
                           "{\"type\":\"delimiter\", \"properties\": {\"delimiter\":\"A\"}},"
                           "{\"type\":\"text\", \"properties\":{\"locale\":\"en_US.UTF-8\",\"case\":\"lower\","
                           "  \"accent\":false,\"stemming\":true,\"stopwords\":[\"fox\"]}},"
                           "{\"type\":\"norm\", \"properties\": {\"locale\":\"en_US.UTF-8\", \"case\":\"upper\"}}"
                        "]}";
  std::string normalized;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(normalized, "pipeline",
    irs::type<irs::text_format::json>::get(), config));
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), normalized);
  ASSERT_NE(nullptr, stream);
  const analyzer_tokens expected{
    {"QUICK", 0, 5, 0},
    {"BROWN", 6, 11, 1},
    {"JUMP", 16, 21, 2}
  };
  assert_pipeline(stream.get(), "QuickABrownAFOXAjUmps", expected);
}

TEST(pipeline_token_stream_test, test_construct_invalid_json) {
  std::string config = "INVALID_JSON}";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_EQ(nullptr, stream);
}

TEST(pipeline_token_stream_test, test_construct_not_object_json) {
  std::string config = "[1,2,3]";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_EQ(nullptr, stream);
}

TEST(pipeline_token_stream_test, test_construct_no_pipeline) {
  std::string config = "{\"NOT_pipeline\":["
                           "{\"type\":\"delimiter\", \"properties\": {\"delimiter\":\"A\"}},"
                           "{\"type\":\"text\", \"properties\":{\"locale\":\"en_US.UTF-8\",\"case\":\"lower\","
                           "  \"accent\":false,\"stemming\":true,\"stopwords\":[\"fox\"]}},"
                           "{\"type\":\"norm\", \"properties\": {\"locale\":\"en_US.UTF-8\", \"case\":\"upper\"}}"
                        "]}";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_EQ(nullptr, stream);
}

TEST(pipeline_token_stream_test, test_construct_not_array_pipeline) {
  std::string config = "{\"pipeline\": \"text\"}";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_EQ(nullptr, stream);
}

TEST(pipeline_token_stream_test, test_construct_not_pipeline_objects) {
  std::string config = "{\"pipeline\":[\"123\"]}";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_EQ(nullptr, stream);
}

TEST(pipeline_token_stream_test, test_construct_no_type) {
  std::string config = "{\"pipeline\":["
                           "{\"type\":\"delimiter\", \"properties\": {\"delimiter\":\"A\"}},"
                           "{\"properties\":{\"locale\":\"en_US.UTF-8\",\"case\":\"lower\","
                           "  \"accent\":false,\"stemming\":true,\"stopwords\":[\"fox\"]}},"
                           "{\"type\":\"norm\", \"properties\": {\"locale\":\"en_US.UTF-8\", \"case\":\"upper\"}}"
                        "]}";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_EQ(nullptr, stream);
}

TEST(pipeline_token_stream_test, test_construct_non_string_type) {
  std::string config = "{\"pipeline\":[{\"type\":1, \"properties\": {\"delimiter\":\"A\"}}]}";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_EQ(nullptr, stream);
}

TEST(pipeline_token_stream_test, test_construct_no_properties) {
  std::string config = "{\"pipeline\":["
                           "{\"type\":\"delimiter\", \"properties\": {\"delimiter\":\"A\"}},"
                           "{\"type\":\"text\"}"
                        "]}";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_EQ(nullptr, stream);
}

TEST(pipeline_token_stream_test, test_construct_invalid_analyzer) {
  std::string config = "{\"pipeline\":["
                           "{\"type\":\"UNKNOWN\", \"properties\": {\"delimiter\":\"A\"}},"
                           "{\"properties\":{\"locale\":\"en_US.UTF-8\",\"case\":\"lower\","
                           "  \"accent\":false,\"stemming\":true,\"stopwords\":[\"fox\"]}},"
                           "{\"type\":\"norm\", \"properties\": {\"locale\":\"en_US.UTF-8\", \"case\":\"upper\"}}"
                        "]}";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_EQ(nullptr, stream);
}

TEST(pipeline_token_stream_test, empty_pipeline_construct) {
  std::string config = "{\"pipeline\":[]}";
  auto stream = irs::analysis::analyzers::get("pipeline", irs::type<irs::text_format::json>::get(), config);
  ASSERT_EQ(nullptr, stream);
}

TEST(pipeline_token_stream_test, normalize_json) {
  //with unknown parameter
  {
    std::string config = "{ \"unknown_parameter\":123,  \"pipeline\":[{\"type\":\"delimiter\", \"properties\": {\"delimiter\":\"A\"}}]}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "pipeline", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"pipeline\":[{\"type\":\"delimiter\",\"properties\":{\"delimiter\":\"A\"}}]}", actual);
  }
  //with unknown parameter in pipeline member
  {
    std::string config = "{\"pipeline\":[{\"unknown_parameter\":123, \"type\":\"delimiter\", \"properties\": {\"delimiter\":\"A\"}}]}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "pipeline", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"pipeline\":[{\"type\":\"delimiter\",\"properties\":{\"delimiter\":\"A\"}}]}", actual);
  }
  //with unknown parameter in analyzer properties
  {
    std::string config = "{\"pipeline\":[{\"type\":\"delimiter\", \"properties\": {\"unknown_paramater\":123, \"delimiter\":\"A\"}}]}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "pipeline", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"pipeline\":[{\"type\":\"delimiter\",\"properties\":{\"delimiter\":\"A\"}}]}", actual);
  }

  //with unknown analyzer
  {
    std::string config = "{\"pipeline\":[{\"type\":\"unknown\", \"properties\": {\"unknown_paramater\":123, \"delimiter\":\"A\"}}]}";
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "pipeline", irs::type<irs::text_format::json>::get(), config));
  }

  //with invalid properties
  {
    std::string config = "{\"pipeline\":[{\"type\":\"delimiter\", \"properties\": {\"wrong_delimiter\":\"A\"}}]}";
    std::string actual;
    ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "pipeline", irs::type<irs::text_format::json>::get(), config));
  }
}

TEST(pipeline_token_stream_test, analyzers_with_payload_offset) {
  irs::bytes_ref test_payload{ { 0x1, 0x2, 0x3 }, 3 };
  irs::bytes_ref test_payload2{ { 0x11, 0x22, 0x33 }, 3 };
  pipeline_test_analyzer payload_offset(true, test_payload);
  pipeline_test_analyzer only_payload(false, test_payload2);
  pipeline_test_analyzer only_offset(true, irs::bytes_ref::NIL);
  pipeline_test_analyzer no_payload_no_offset(false, irs::bytes_ref::NIL);

  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.emplace_back(irs::analysis::analyzer::ptr(), &payload_offset);
    pipeline_options.emplace_back(irs::analysis::analyzer::ptr(), &only_offset);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    auto* offset = irs::get<irs::offset>(pipe);
    ASSERT_TRUE(offset);
    auto* term = irs::get<irs::term_attribute>(pipe);
    ASSERT_TRUE(term);
    auto* inc = irs::get<irs::increment>(pipe);
    ASSERT_TRUE(inc);
    auto* pay = irs::get<irs::payload>(pipe);
    ASSERT_TRUE(pay);
    ASSERT_TRUE(pipe.reset("A"));
    ASSERT_TRUE(pipe.next());
    ASSERT_EQ(test_payload, pay->value);
  }
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.emplace_back(irs::analysis::analyzer::ptr(), &only_offset);
    pipeline_options.emplace_back(irs::analysis::analyzer::ptr(), &payload_offset);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    auto* offset = irs::get<irs::offset>(pipe);
    ASSERT_TRUE(offset);
    auto* term = irs::get<irs::term_attribute>(pipe);
    ASSERT_TRUE(term);
    auto* inc = irs::get<irs::increment>(pipe);
    ASSERT_TRUE(inc);
    auto* pay = irs::get<irs::payload>(pipe);
    ASSERT_TRUE(pay);
    ASSERT_TRUE(pipe.reset("A"));
    ASSERT_TRUE(pipe.next());
    ASSERT_EQ(test_payload, pay->value);
  }
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.emplace_back(irs::analysis::analyzer::ptr(), &payload_offset);
    pipeline_options.emplace_back(irs::analysis::analyzer::ptr(), &only_payload);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    auto* offset = irs::get<irs::offset>(pipe);
    ASSERT_FALSE(offset);
    auto* term = irs::get<irs::term_attribute>(pipe);
    ASSERT_TRUE(term);
    auto* inc = irs::get<irs::increment>(pipe);
    ASSERT_TRUE(inc);
    auto* pay = irs::get<irs::payload>(pipe);
    ASSERT_TRUE(pay);
    ASSERT_TRUE(pipe.reset("A"));
    ASSERT_TRUE(pipe.next());
    ASSERT_EQ(test_payload2, pay->value);
  }
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.emplace_back(irs::analysis::analyzer::ptr(), &only_payload);
    pipeline_options.emplace_back(irs::analysis::analyzer::ptr(), &payload_offset);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    auto* offset = irs::get<irs::offset>(pipe);
    ASSERT_FALSE(offset);
    auto* term = irs::get<irs::term_attribute>(pipe);
    ASSERT_TRUE(term);
    auto* inc = irs::get<irs::increment>(pipe);
    ASSERT_TRUE(inc);
    auto* pay = irs::get<irs::payload>(pipe);
    ASSERT_TRUE(pay);
    ASSERT_TRUE(pipe.reset("A"));
    ASSERT_TRUE(pipe.next());
    ASSERT_EQ(test_payload, pay->value);
  }
  {
    irs::analysis::pipeline_token_stream::options_t pipeline_options;
    pipeline_options.emplace_back(irs::analysis::analyzer::ptr(), &only_payload);
    pipeline_options.emplace_back(irs::analysis::analyzer::ptr(), &no_payload_no_offset);
    irs::analysis::pipeline_token_stream pipe(std::move(pipeline_options));
    auto* offset = irs::get<irs::offset>(pipe);
    ASSERT_FALSE(offset);
    auto* term = irs::get<irs::term_attribute>(pipe);
    ASSERT_TRUE(term);
    auto* inc = irs::get<irs::increment>(pipe);
    ASSERT_TRUE(inc);
    auto* pay = irs::get<irs::payload>(pipe);
    ASSERT_TRUE(pay);
    ASSERT_TRUE(pipe.reset("A"));
    ASSERT_TRUE(pipe.next());
    ASSERT_EQ(test_payload2, pay->value);
  }
}

#endif
