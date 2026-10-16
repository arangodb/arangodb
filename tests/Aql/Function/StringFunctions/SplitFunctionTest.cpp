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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Function/StringFunctions.h"
#include "Basics/voc-errors.h"
#include "StringFunctionTestHelper.h"

#include <string>

namespace arangodb::tests::aql {

using arangodb::aql::functions::split;

namespace {
// "0,1,...,count-1"
std::string numberList(int count) {
  std::string out;
  for (int i = 0; i < count; ++i) {
    if (i > 0) {
      out += ',';
    }
    out += std::to_string(i);
  }
  return out;
}

std::string quotedNumberList(int count) {
  std::string out;
  for (int i = 0; i < count; ++i) {
    if (i > 0) {
      out += ',';
    }
    out += '"' + std::to_string(i) + '"';
  }
  return out;
}
}  // namespace

TEST_F(StringFunctionTest, split_separates_text_at_each_separator) {
  expectJson(split(ctx, str("a,b,c"), &str(","), nullptr), R"(["a","b","c"])");
  expectJson(split(ctx, str("abc"), &str(","), nullptr), R"(["abc"])");
  expectJson(split(ctx, str("a--b---c"), &str("--"), nullptr),
             R"(["a","b","-c"])");
}

TEST_F(StringFunctionTest, split_keeps_empty_fields) {
  expectJson(split(ctx, str("a,"), &str(","), nullptr), R"(["a",""])");
  expectJson(split(ctx, str(",a"), &str(","), nullptr), R"(["","a"])");
  expectJson(split(ctx, str("a,,b"), &str(","), nullptr), R"(["a","","b"])");
  expectJson(split(ctx, str(","), &str(","), nullptr), R"(["",""])");
}

TEST_F(StringFunctionTest, split_empty_text_gives_one_empty_string) {
  expectJson(split(ctx, str(""), &str(","), nullptr), R"([""])");
  expectJson(split(ctx, str(""), &str(""), nullptr), R"([])");
}

TEST_F(StringFunctionTest, split_empty_separator_splits_into_code_points) {
  expectJson(split(ctx, str("abc"), &str(""), nullptr), R"(["a","b","c"])");
  expectJson(split(ctx, str("a😀b"), &str(""), nullptr), R"(["a","😀","b"])");
  expectJson(split(ctx, str("e\xcc\x81x"), &str(""), nullptr),
             "[\"e\",\"\xcc\x81\",\"x\"]");
  expectJson(split(ctx, str("a\r\nb"), &str(""), nullptr),
             R"(["a","\r","\n","b"])");
}

TEST_F(StringFunctionTest, split_separator_regex_characters_are_literal) {
  expectJson(split(ctx, str("a.b|c"), &str("."), nullptr), R"(["a","b|c"])");
  expectJson(split(ctx, str("a.b|c"), &str("|"), nullptr), R"(["a.b","c"])");
  expectJson(split(ctx, str("a]b"), &str("]"), nullptr), R"(["a","b"])");
  expectJson(split(ctx, str("a\\b"), &str("\\"), nullptr), R"(["a","b"])");
  expectJson(split(ctx, str("a(b)*c"), &str("(b)*"), nullptr), R"(["a","c"])");
}

TEST_F(StringFunctionTest, split_array_separator_splits_at_any_element) {
  expectJson(split(ctx, str("a,b;c"), &json(R"([",", ";"])"), nullptr),
             R"(["a","b","c"])");
}

TEST_F(StringFunctionTest, split_array_separator_prefers_earlier_elements) {
  expectJson(split(ctx, str("xaby"), &json(R"(["a", "ab"])"), nullptr),
             R"(["x","by"])");
  expectJson(split(ctx, str("xaby"), &json(R"(["ab", "a"])"), nullptr),
             R"(["x","y"])");
}

TEST_F(StringFunctionTest,
       split_array_with_empty_or_non_string_element_splits_into_characters) {
  expectJson(split(ctx, str("a,b"), &json(R"([",", ""])"), nullptr),
             R"(["a",",","b"])");
  expectJson(split(ctx, str("a1b"), &json(R"([",", 1])"), nullptr),
             R"(["a","1","b"])");
}

TEST_F(StringFunctionTest,
       split_non_string_separator_splits_into_characters_with_empty_ends) {
  expectJson(split(ctx, str("abc"), &json("null"), nullptr),
             R"(["","a","b","c",""])");
  expectJson(split(ctx, str("a1b"), &json("1"), nullptr),
             R"(["","a","1","b",""])");
  expectJson(split(ctx, str("ab"), &json("true"), nullptr),
             R"(["","a","b",""])");
  expectJson(split(ctx, str("ab"), &json("[]"), nullptr), R"(["","a","b",""])");
}

TEST_F(StringFunctionTest, split_object_separator_is_a_type_mismatch) {
  expectError(split(ctx, str("abc"), &json("{}"), nullptr),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(StringFunctionTest, split_without_separator_wraps_the_value) {
  expectJson(split(ctx, str("a,b"), nullptr, nullptr), R"(["a,b"])");
  expectJson(split(ctx, json("12"), nullptr, nullptr), "[12]");
  expectJson(split(ctx, json("[1,2]"), nullptr, nullptr), "[[1,2]]");
}

TEST_F(StringFunctionTest, split_non_string_values_are_stringified) {
  expectJson(split(ctx, json("null"), &str(","), nullptr), R"([""])");
  expectJson(split(ctx, json("1.5"), &str("."), nullptr), R"(["1","5"])");
  expectJson(split(ctx, json("true"), &str("r"), nullptr), R"(["t","ue"])");
  expectJson(split(ctx, json("[1,2]"), &str(","), nullptr), R"(["[1","2]"])");
  expectJson(split(ctx, json(R"({"a":1,"b":2})"), &str(","), nullptr),
             R"(["{\"a\":1","\"b\":2}"])");
}

TEST_F(StringFunctionTest, split_limit_caps_the_number_of_fields) {
  expectJson(split(ctx, str("a,b,c"), &str(","), &json("1")), R"(["a"])");
  expectJson(split(ctx, str("a,b,c"), &str(","), &json("1.9")), R"(["a"])");
  expectJson(split(ctx, str("a,b"), &str(","), &json("5")), R"(["a","b"])");
  expectJson(split(ctx, str("abc"), &str(""), &json("2")), R"(["a","b"])");
}

TEST_F(StringFunctionTest, split_zero_limit_gives_empty_array) {
  expectJson(split(ctx, str("a,b"), &str(","), &json("0")), "[]");
}

TEST_F(StringFunctionTest, split_negative_limit_gives_null) {
  auto result = split(ctx, str("a,b"), &str(","), &json("-1"));
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result->isNull(false));
}

TEST_F(StringFunctionTest, split_non_numeric_limit_is_a_type_mismatch) {
  expectError(split(ctx, str("a,b"), &str(","), &str("1")),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(split(ctx, str("a,b"), &str(","), &json("null")),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(StringFunctionTest, split_handles_more_fields_than_one_icu_batch) {
  for (int count : {15, 16, 17, 33}) {
    expectJson(split(ctx, str(numberList(count)), &str(","), nullptr),
               "[" + quotedNumberList(count) + "]");
  }
  std::string characters = "[";
  for (int i = 0; i < 33; ++i) {
    characters += i > 0 ? R"(,"x")" : R"("x")";
  }
  expectJson(split(ctx, str(std::string(33, 'x')), &str(""), nullptr),
             characters + "]");
}

TEST_F(StringFunctionTest, split_drops_trailing_empty_field_after_fifteen) {
  expectJson(split(ctx, str(numberList(14) + ","), &str(","), nullptr),
             "[" + quotedNumberList(14) + R"(,""])");
  expectJson(split(ctx, str(numberList(15) + ","), &str(","), nullptr),
             "[" + quotedNumberList(15) + "]");
  expectJson(split(ctx, str(numberList(16) + ","), &str(","), nullptr),
             "[" + quotedNumberList(16) + R"(,""])");
}

}  // namespace arangodb::tests::aql
