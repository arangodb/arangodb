////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
#include "utils/locale_utils.hpp"

#include "analysis/segmentation_token_stream.hpp"

namespace {

struct analyzer_token {
  irs::string_ref value;
  size_t start;
  size_t end;
  uint32_t pos;
};

using analyzer_tokens = std::vector<analyzer_token>;

std::basic_string<wchar_t> utf_to_utf(const irs::bytes_ref& value) {
  auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
  std::basic_string<wchar_t> result;

  if (!irs::locale_utils::append_internal<wchar_t>(result, irs::ref_cast<char>(value), locale)) {
    throw irs::illegal_state(); // cannot use ASSERT_TRUE(...) here, therefore throw
  }

  return result;
}

} // namespace

void assert_stream(irs::analysis::analyzer* pipe, const std::string& data, const analyzer_tokens& expected_tokens) {
  SCOPED_TRACE(data);
  auto* offset = irs::get<irs::offset>(*pipe);
  ASSERT_TRUE(offset);
  auto* term = irs::get<irs::term_attribute>(*pipe);
  ASSERT_TRUE(term);
  auto* inc = irs::get<irs::increment>(*pipe);
  ASSERT_TRUE(inc);
  ASSERT_TRUE(pipe->reset(data));
  uint32_t pos{ std::numeric_limits<uint32_t>::max() };
  auto expected_token = expected_tokens.begin();
  while (pipe->next()) {
    auto term_value = std::string(irs::ref_cast<char>(term->value).c_str(), term->value.size());
    SCOPED_TRACE(testing::Message("Term:<") << term_value << ">");
    SCOPED_TRACE(testing::Message("Expected term:<") << expected_token->value << ">");
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
  ASSERT_FALSE(pipe->next());
}


TEST(segmentation_token_stream_test, consts) {
  static_assert("segmentation" == irs::type<irs::analysis::segmentation_token_stream>::name());
}

TEST(segmentation_token_stream_test, alpha_no_case_test) {
  irs::analysis::segmentation_token_stream::options_t opt;
  opt.case_convert = irs::analysis::segmentation_token_stream::options_t::case_convert_t::NONE;
  irs::analysis::segmentation_token_stream stream(std::move(opt));
  auto* term = irs::get<irs::term_attribute>(stream);
  std::string data = "File:Constantinople(1878)-Turkish Goverment information brocure (1950s) - Istanbul coffee house.png";
  const analyzer_tokens expected{
    { "File:Constantinople", 0, 19, 0},
    { "1878", 20, 24, 1},
    { "Turkish", 26, 33, 2},
    { "Goverment", 34, 43, 3},
    { "information", 44, 55, 4},
    { "brocure", 56, 63, 5},
    { "1950s", 65, 70, 6},
    { "Istanbul", 74, 82, 7},
    { "coffee", 83, 89, 8},
    { "house.png", 90, 99, 9}
  };
  assert_stream(&stream, data, expected);
}

TEST(segmentation_token_stream_test, alpha_lower_case_test) {
  irs::analysis::segmentation_token_stream::options_t opt; // LOWER is default
  irs::analysis::segmentation_token_stream stream(std::move(opt));
  auto* term = irs::get<irs::term_attribute>(stream);
  std::string data = "File:Constantinople(1878)-Turkish Goverment information brocure (1950s) - Istanbul coffee house.png";
  const analyzer_tokens expected{
    { "file:constantinople", 0, 19, 0},
    { "1878", 20, 24, 1},
    { "turkish", 26, 33, 2},
    { "goverment", 34, 43, 3},
    { "information", 44, 55, 4},
    { "brocure", 56, 63, 5},
    { "1950s", 65, 70, 6},
    { "istanbul", 74, 82, 7},
    { "coffee", 83, 89, 8},
    { "house.png", 90, 99, 9}
  };
  assert_stream(&stream, data, expected);
}

TEST(segmentation_token_stream_test, alpha_upper_case_test) {
  irs::analysis::segmentation_token_stream::options_t opt;
  opt.case_convert = irs::analysis::segmentation_token_stream::options_t::case_convert_t::UPPER;
  irs::analysis::segmentation_token_stream stream(std::move(opt));
  auto* term = irs::get<irs::term_attribute>(stream);
  std::string data = "File:Constantinople(1878)-Turkish Goverment information brocure (1950s) - Istanbul coffee house.png";
  const analyzer_tokens expected{
    { "FILE:CONSTANTINOPLE", 0, 19, 0},
    { "1878", 20, 24, 1},
    { "TURKISH", 26, 33, 2},
    { "GOVERMENT", 34, 43, 3},
    { "INFORMATION", 44, 55, 4},
    { "BROCURE", 56, 63, 5},
    { "1950S", 65, 70, 6},
    { "ISTANBUL", 74, 82, 7},
    { "COFFEE", 83, 89, 8},
    { "HOUSE.PNG", 90, 99, 9}
  };
  assert_stream(&stream, data, expected);
}

TEST(segmentation_token_stream_test, graphic_upper_case_test) {
  irs::analysis::segmentation_token_stream::options_t opt;
  opt.case_convert = irs::analysis::segmentation_token_stream::options_t::case_convert_t::UPPER;
  opt.word_break = irs::analysis::segmentation_token_stream::options_t::word_break_t::GRAPHIC;
  irs::analysis::segmentation_token_stream stream(std::move(opt));
  auto* term = irs::get<irs::term_attribute>(stream);
  std::string data = "File:Constantinople(1878)-Turkish Goverment information brocure (1950s) - Istanbul coffee house.png";
  const analyzer_tokens expected{
    { "FILE:CONSTANTINOPLE", 0, 19, 0},
    { "(", 19, 20, 1},
    { "1878", 20, 24, 2},
    { ")", 24, 25, 3},
    { "-", 25, 26, 4},
    { "TURKISH", 26, 33, 5},
    { "GOVERMENT", 34, 43, 6},
    { "INFORMATION", 44, 55, 7},
    { "BROCURE", 56, 63, 8},
    { "(", 64, 65, 9},
    { "1950S", 65, 70, 10},
    { ")", 70, 71, 11},
    { "-", 72, 73, 12},
    { "ISTANBUL", 74, 82, 13},
    { "COFFEE", 83, 89, 14},
    { "HOUSE.PNG", 90, 99, 15}
  };
  assert_stream(&stream, data, expected);
}

TEST(segmentation_token_stream_test, all_lower_case_test) {
  irs::analysis::segmentation_token_stream::options_t opt;
  opt.case_convert = irs::analysis::segmentation_token_stream::options_t::case_convert_t::LOWER;
  opt.word_break = irs::analysis::segmentation_token_stream::options_t::word_break_t::ALL;
  irs::analysis::segmentation_token_stream stream(std::move(opt));
  auto* term = irs::get<irs::term_attribute>(stream);
  std::string data = "File:Constantinople(1878)-Turkish Goverment information brocure (1950s) - Istanbul coffee house.png";
  const analyzer_tokens expected{
    { "file:constantinople", 0, 19, 0},
    { "(", 19, 20, 1},
    { "1878", 20, 24, 2},
    { ")", 24, 25, 3},
    { "-", 25, 26, 4},
    { "turkish", 26, 33, 5},
    { " ", 33, 34, 6},
    { "goverment", 34, 43, 7},
    { " ", 43, 44, 8},
    { "information", 44, 55, 9},
    { " ", 55, 56, 10},
    { "brocure", 56, 63, 11},
    { " ", 63, 64, 12},
    { "(", 64, 65, 13},
    { "1950s", 65, 70, 14},
    { ")", 70, 71, 15},
    { " ", 71, 72, 16},
    { "-", 72, 73, 17},
    { " ", 73, 74, 18},
    { "istanbul", 74, 82, 19},
    { " ", 82, 83, 20},
    { "coffee", 83, 89, 21},
    { " ", 89, 90, 22},
    { "house.png", 90, 99, 23}
  };
  assert_stream(&stream, data, expected);
}

TEST(segmentation_token_stream_test, chinese_glyphs_test) {
  std::wstring sDataUCS2 = L"\u4ECA\u5929\u4E0B\u5348\u7684\u592A\u9633\u5F88\u6E29\u6696\u3002";
  auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
  std::string data;
  ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
  irs::analysis::segmentation_token_stream::options_t opt;
  irs::analysis::segmentation_token_stream stream(std::move(opt));

  auto testFunc = [](const irs::string_ref& data, irs::analysis::analyzer* pStream) {
    ASSERT_TRUE(pStream->reset(data));
    auto* pOffset = irs::get<irs::offset>(*pStream);
    ASSERT_NE(nullptr, pOffset);
    auto* pPayload = irs::get<irs::payload>(*pStream);
    ASSERT_EQ(nullptr, pPayload);
    auto* pValue = irs::get<irs::term_attribute>(*pStream);
    ASSERT_NE(nullptr, pValue);

    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u4ECA", utf_to_utf(pValue->value));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u5929", utf_to_utf(pValue->value));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u4E0B", utf_to_utf(pValue->value));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u5348", utf_to_utf(pValue->value));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u7684", utf_to_utf(pValue->value));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u592A", utf_to_utf(pValue->value));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u9633", utf_to_utf(pValue->value));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u5F88", utf_to_utf(pValue->value));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u6E29", utf_to_utf(pValue->value));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u6696", utf_to_utf(pValue->value));
    ASSERT_FALSE(pStream->next());
  };

  testFunc(data, &stream);
}

TEST(segmentation_token_stream_test, make_empty_object) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{}");
  ASSERT_TRUE(stream);
  const analyzer_tokens expected{
    { "test", 0, 4, 0},
    { "retest", 7, 13, 1}
  };
  std::string data = "Test - ReTeSt";
  assert_stream(stream.get(), data, expected);
}

TEST(segmentation_token_stream_test, make_lowercase) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"case\":\"lower\"}");
  ASSERT_TRUE(stream);
  const analyzer_tokens expected{
    { "test", 0, 4, 0},
    { "retest", 7, 13, 1}
  };
  std::string data = "Test - ReTeSt";
  assert_stream(stream.get(), data, expected);
}

TEST(segmentation_token_stream_test, make_nonecase) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"case\":\"none\"}");
  ASSERT_TRUE(stream);
  const analyzer_tokens expected{
    { "Test", 0, 4, 0},
    { "ReTeSt", 7, 13, 1}
  };
  std::string data = "Test - ReTeSt";
  assert_stream(stream.get(), data, expected);
}

TEST(segmentation_token_stream_test, make_uppercase) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"case\":\"upper\"}");
  ASSERT_TRUE(stream);
  const analyzer_tokens expected{
    { "TEST", 0, 4, 0},
    { "RETEST", 7, 13, 1}
  };
  std::string data = "Test - ReTeSt";
  assert_stream(stream.get(), data, expected);
}

TEST(segmentation_token_stream_test, make_invalidcase) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"case\":\"invalid\"}");
  ASSERT_FALSE(stream);
}


TEST(segmentation_token_stream_test, make_numbercase) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"case\":2}");
  ASSERT_FALSE(stream);
}

TEST(segmentation_token_stream_test, make_uppercase_alphabreak) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"case\":\"upper\", \"break\":\"alpha\"}");
  ASSERT_TRUE(stream);
  const analyzer_tokens expected{
    { "TEST", 0, 4, 0},
    { "RETEST", 7, 13, 1}
  };
  std::string data = "Test - ReTeSt";
  assert_stream(stream.get(), data, expected);
}

TEST(segmentation_token_stream_test, make_uppercase_all_break) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"case\":\"upper\", \"break\":\"all\"}");
  ASSERT_TRUE(stream);
  const analyzer_tokens expected{
    { "TEST", 0, 4, 0},
    { " ", 4, 5, 1},
    { "-", 5, 6, 2},
    { " ", 6, 7, 3},
    { "RETEST", 7, 13, 4}
  };
  std::string data = "Test - ReTeSt";
  assert_stream(stream.get(), data, expected);
}

TEST(segmentation_token_stream_test, make_uppercase_graphic_break) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"case\":\"upper\", \"break\":\"graphic\"}");
  ASSERT_TRUE(stream);
  const analyzer_tokens expected{
    { "TEST", 0, 4, 0},
    { "-", 5, 6, 1},
    { "RETEST", 7, 13, 2}
  };
  std::string data = "Test - ReTeSt";
  assert_stream(stream.get(), data, expected);
}

TEST(segmentation_token_stream_test, make_uppercase_invalid_break) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"case\":\"upper\", \"break\":\"_INVALID_\"}");
  ASSERT_FALSE(stream);
}

TEST(segmentation_token_stream_test, make_uppercase_invalid_number_break) {
  auto stream = irs::analysis::analyzers::get(
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"case\":\"upper\", \"break\":1}");
  ASSERT_FALSE(stream);
}

TEST(segmentation_token_stream_test, make_invalid_json) {
  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("segmentation", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("segmentation", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("segmentation", irs::type<irs::text_format::json>::get(), "\"abc\""));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("segmentation", irs::type<irs::text_format::json>::get(), "{\"case\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("segmentation", irs::type<irs::text_format::json>::get(), "{\"break\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("segmentation", irs::type<irs::text_format::json>::get(), "{\"case\":1, \"break\":\"all\"}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("segmentation", irs::type<irs::text_format::json>::get(), "{\"case\":\"none\", \"break\":1}"));
  }
}

TEST(segmentation_token_stream_test, normalize_empty_object) {
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(
      actual,
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{}"));
  ASSERT_EQ(actual, "{\n  \"break\" : \"alpha\",\n  \"case\" : \"lower\"\n}");
}

TEST(segmentation_token_stream_test, normalize_all_none_values) {
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(
      actual,
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"break\":\"all\", \"case\":\"none\"}"));
  ASSERT_EQ(actual, "{\n  \"break\" : \"all\",\n  \"case\" : \"none\"\n}");
}

TEST(segmentation_token_stream_test, normalize_graph_upper_values) {
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(
      actual,
      "segmentation",
      irs::type<irs::text_format::json>::get(),
      "{\"break\":\"graphic\", \"case\":\"upper\"}"));
  ASSERT_EQ(actual, "{\n  \"break\" : \"graphic\",\n  \"case\" : \"upper\"\n}");
}

TEST(segmentation_token_stream_test, normalize_invalid) {
  std::string actual;
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "segmentation",
    irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "segmentation",
    irs::type<irs::text_format::json>::get(), "1"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "segmentation",
    irs::type<irs::text_format::json>::get(), "\"abc\""));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "segmentation",
    irs::type<irs::text_format::json>::get(), "{\"case\":1}"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "segmentation",
    irs::type<irs::text_format::json>::get(), "{\"break\":1}"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "segmentation",
    irs::type<irs::text_format::json>::get(), "{\"case\":1, \"break\":\"all\"}"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "segmentation",
    irs::type<irs::text_format::json>::get(), "{\"case\":\"none\", \"break\":1}"));
}
