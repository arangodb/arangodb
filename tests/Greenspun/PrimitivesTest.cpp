#include <iostream>

#include "Greenspun/Interpreter.h"
#include "Greenspun/Primitives.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

#include "gtest/gtest.h"

#include <numeric>

/*
 * Calculation operators
 */

using namespace arangodb::greenspun;

struct GreenspunTest : public ::testing::Test {
  Machine m;
  VPackBuilder result;

  GreenspunTest() { InitMachine(m); }
};

TEST_F(GreenspunTest, dict_x_tract_value_found) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-x-tract", {"foo":1, "bar":3}, "foo"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isObject());
  ASSERT_EQ(result.slice().get("foo").getNumericValue<double>(), 1);
  ASSERT_TRUE(result.slice().get("bar").isNone());
}

TEST_F(GreenspunTest, dict_x_tract_value_not_found) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-x-tract", {"foo":1, "bar":3}, "baz"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, dict_x_tract_x_value_found) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-x-tract-x", {"foo":1, "bar":3}, "foo"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isObject());
  ASSERT_EQ(result.slice().get("foo").getNumericValue<double>(), 1);
  ASSERT_TRUE(result.slice().get("bar").isNone());
}

TEST_F(GreenspunTest, dict_x_tract_x_value_not_found) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-x-tract-x", {"foo":1, "bar":3}, "baz"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.ok());
  ASSERT_TRUE(result.slice().isEmptyObject());
}

/*
 * Arithmetic Operators
 */

TEST_F(GreenspunTest, add_int) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["+", 1, 1]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_EQ(2, result.slice().getDouble());
}

TEST_F(GreenspunTest, add_double) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["+", 1.1, 2.1]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_EQ(3.2, result.slice().getDouble());
}

TEST_F(GreenspunTest, sub_int) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 1, 1]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_EQ(0, result.slice().getDouble());
}

TEST_F(GreenspunTest, sub_double) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 4.4, 1.2]
    )aql");

  Evaluate(m, program->slice(), result);
  // TODO: also do more precise double comparison here
  ASSERT_EQ(3.2, result.slice().getDouble());
}

TEST_F(GreenspunTest, sub_int_negative) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 2, 4]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_EQ(-2, result.slice().getDouble());
}

TEST_F(GreenspunTest, mul_int) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["*", 2, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_EQ(4, result.slice().getDouble());
}

TEST_F(GreenspunTest, mul_zero) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["*", 2, 0]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_EQ(0, result.slice().getDouble());
}

TEST_F(GreenspunTest, div_double) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["/", 2, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(1 == result.slice().getDouble());
}

TEST_F(GreenspunTest, div_zero) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["/", 2, 0]
    )aql");

  EvalResult res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

/*
 * Logical Operators
 */

TEST_F(GreenspunTest, unary_not_true) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["not", true]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, unary_not_false) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["not", false]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, false_huh_true) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["false?", true]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, false_huh_false) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["false?", false]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, false_huh_int) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["false?", 12]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, true_huh_true) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["true?", true]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, true_huh_false) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["true?", false]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

/*
 * Comparison operators
 */

TEST_F(GreenspunTest, eq_huh_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 2, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, eq_huh_not_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 3, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, eq_huh_equal_double) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 2.2, 2.2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, eq_huh_not_equal_double) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 2.4, 2.2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, eq_huh_true_true) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", true, true]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, eq_huh_true_false) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", true, false]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, eq_huh_equal_string) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", "hello", "hello"]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, eq_huh_not_equal_string) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", "hello", "world"]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, gt_int_greater) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 2, 1]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, gt_int_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 2, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, gt_int_less) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 1, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, gt_double_greater) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 2.4, 1.3]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, gt_double_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 2.4, 2.4]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, gt_double_less) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 1.1, 2.3]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, gt_error_bool) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", true, false]
    )aql");

  EvalResult res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, gt_error_string) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", "astring", "bstring"]
    )aql");

  EvalResult res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, ge_int_greater) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2, 1]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ge_int_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ge_int_less) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 1, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ge_double_greater) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2.4, 1.3]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ge_double_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2.4, 2.4]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ge_double_less) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 1.1, 2.3]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ge_error_bool) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", true, false]
    )aql");

  EvalResult res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, ge_error_string) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", "astring", "bstring"]
    )aql");

  EvalResult res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, le_int_greater) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 2, 1]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, le_int_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 2, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, le_int_less) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 1, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, le_double_greater) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 2.4, 1.3]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, le_double_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 2.4, 2.4]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, le_double_less) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 1.1, 2.3]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, le_error_bool) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", true, false]
    )aql");

  EvalResult res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, le_error_string) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", "astring", "bstring"]
    )aql");

  EvalResult res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, lt_int_greater) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 2, 1]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, lt_int_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 2, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, lt_int_less) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 1, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, lt_double_greater) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 2.4, 1.3]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, lt_double_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 2.4, 2.4]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, lt_double_less) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 1.1, 2.3]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, lt_error_bool) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", true, false]
    )aql");

  EvalResult res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, lt_error_string) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", "astring", "bstring"]
    )aql");

  EvalResult res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, ne_int) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 2, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ne_int_not_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 3, 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ne_doubles) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 2.2, 2.2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ne_doubles_not_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 2.4, 2.2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ne_bool) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", true, true]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ne_bool_not_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", true, false]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}
TEST_F(GreenspunTest, ne_string) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", "hello", "hello"]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_FALSE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, ne_string_not_equal) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", "hello", "world"]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().getBoolean());
}

TEST_F(GreenspunTest, string_huh) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["string?", "hello"]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isTrue());

  program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["string?", 12]
    )aql");

  result.clear();
  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isFalse()) << result.slice().toJson();
}

TEST_F(GreenspunTest, bool_huh) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["bool?", true]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isTrue());

  program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["bool?", "12"]
    )aql");

  result.clear();
  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isFalse()) << result.slice().toJson();
}

TEST_F(GreenspunTest, number_huh) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["number?", 17]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isTrue());

  program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["number?", "12"]
    )aql");

  result.clear();
  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isFalse()) << result.slice().toJson();
}

/*
 * List operators
 */

TEST_F(GreenspunTest, list_huh) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["list?", ["quote", [1, 2, 3]]]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isTrue());

  program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["list?", 12]
    )aql");

  result.clear();
  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isFalse()) << result.slice().toJson();
}

TEST_F(GreenspunTest, list_cat_single_list) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["list-cat", ["quote", [1, 2, 3]]]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isArray());
  ASSERT_EQ(result.slice().length(), 3);
  for (size_t i = 0; i < 3; i++) {
    ASSERT_EQ(result.slice().at(i).getInt(), (i + 1));
  }
}

TEST_F(GreenspunTest, list_cat_multi_list) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["list-cat", ["quote", [1, 2, 3]], ["quote", [4, 5]]]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isArray());
  ASSERT_EQ(result.slice().length(), 5);
  for (size_t i = 0; i < 5; i++) {
    ASSERT_EQ(result.slice().at(i).getInt(), (i + 1));
  }
}

/*
 * String operators
 */

TEST_F(GreenspunTest, string_cat_single_param) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["string-cat", "hello"]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().toString(), "hello");
}

TEST_F(GreenspunTest, string_cat_multi_param) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["string-cat", "hello", "world"]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().toString(), "helloworld");
}

TEST_F(GreenspunTest, int_to_string) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["int-to-str", 2]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ("2", result.slice().toString());
}

/*
 * Access operators
 */

TEST_F(GreenspunTest, attrib_ref_existing) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-ref", {"hello": 1}, "hello"]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_EQ(result.slice().getNumber<int>(), 1);
}

TEST_F(GreenspunTest, attrib_ref_non_existing) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-ref", {"XXXX": 1}, "hello"]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isNone());
}

TEST_F(GreenspunTest, attrib_ref_or_existing) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-ref-or", {"hello": 1}, "hello", 4]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_EQ(result.slice().getNumber<int>(), 1);
}

TEST_F(GreenspunTest, attrib_ref_or_non_existing) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-ref-or", {"XXXX": 1}, "hello", 4]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_EQ(result.slice().getNumber<int>(), 4);
}

TEST_F(GreenspunTest, attrib_ref_or_fail_existing) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-ref-or-fail", {"hello": 1}, "hello"]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_EQ(result.slice().getNumber<int>(), 1);
}

TEST_F(GreenspunTest, attrib_ref_or_fail_non_existing) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-ref-or-fail", {"XXXX": 1}, "hello"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, var_ref_non_existing) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["var-ref", "peter"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isNull());
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, lambda_constant) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [["lambda", ["quote", []], ["quote", []], 12]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12);
}

TEST_F(GreenspunTest, lambda_expression) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [["lambda", ["quote", []], ["quote", []], ["+", 10, 2]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12);
}

TEST_F(GreenspunTest, lambda_single_param) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [["lambda", ["quote", []], ["quote", ["x"]], ["quote", ["var-ref", "x"]]], 12]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12);
}

TEST_F(GreenspunTest, lambda_multi_param) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [["lambda", ["quote", []], ["quote", ["a", "b"]],
        ["quote", [ "+",
          ["var-ref", "a"],
          ["var-ref", "b"]]]
      ], 10, 2]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12);
}

TEST_F(GreenspunTest, lambda_single_capture) {
  auto v = arangodb::velocypack::Parser::fromJson(R"aql(12)aql");
  m.setVariable("a", v->slice());

  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [
        ["lambda", ["quote", ["a"]], ["quote", []], ["quote", ["var-ref", "a"]]]
      ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12);
}

TEST_F(GreenspunTest, lambda_param_capture) {
  auto v = arangodb::velocypack::Parser::fromJson(R"aql(8)aql");
  m.setVariable("a", v->slice());

  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [
        ["lambda", ["quote", ["a"]], ["quote", ["b"]], ["quote", ["+", ["var-ref", "a"], ["var-ref", "b"]]]],
        4
      ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12);
}

TEST_F(GreenspunTest, lambda_variable_isolation) {
  auto v = arangodb::velocypack::Parser::fromJson(R"aql(8)aql");
  m.setVariable("a", v->slice());

  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [
        ["lambda", ["quote", []], ["quote", []], ["quote", ["var-ref", "a"]]]
      ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, lambda_call_eval_parameter) {
  auto v = arangodb::velocypack::Parser::fromJson(R"aql(8)aql");
  m.setVariable("a", v->slice());

  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [
        ["lambda", ["quote", []], ["quote", ["x"]], ["quote", ["var-ref", "x"]]],
        ["+", 10, 2]
      ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12);
}

TEST_F(GreenspunTest, let_simple_statement) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [], 12]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12.);
}

TEST_F(GreenspunTest, let_seq) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [], 8, 12]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12.);
}

TEST_F(GreenspunTest, let_binding) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [["a", 12]], ["var-ref", "a"]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12.);
}

TEST_F(GreenspunTest, let_double_naming) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [["a", 1], ["a", 12]], ["var-ref", "a"]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, let_multi_binding) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [["a", 1], ["b", 11]], ["+", ["var-ref", "a"], ["var-ref", "b"]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 12.);
}

TEST_F(GreenspunTest, let_invisible_bindings) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [["a", 1], ["b", ["var-ref", "a"]]], ["+", ["var-ref", "a"], ["var-ref", "b"]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, let_error_no_names) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, let_error_no_list) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", "foo"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, let_error_no_pairs) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [[1, 2, 3]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, let_error_no_string_names) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [[1, 2]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, let_error_seq) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [["foo", 2]], ["foo"]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, filter_array) {
  auto program = arangodb::velocypack::Parser::fromJson(R"air(
      ["filter", ["lambda",
        ["quote", []],
        ["quote", ["idx", "value"]],
        ["quote", ["gt?", ["var-ref", "value"], 3]]
      ], ["list", 1, 2, 3, 4, 5, 6]]
    )air");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().toJson(), "[4,5,6]");
}

TEST_F(GreenspunTest, filter_array_keys) {
  auto program = arangodb::velocypack::Parser::fromJson(R"air(
      ["filter", ["lambda",
        ["quote", []],
        ["quote", ["idx", "value"]],
        ["quote", ["gt?", ["var-ref", "idx"], 3]]
      ], ["list", 4, 6, 8, 1, 7, 3]]
    )air");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().toJson(), "[7,3]");
}

TEST_F(GreenspunTest, filter_dict) {
  auto program = arangodb::velocypack::Parser::fromJson(R"air(
      ["filter", ["lambda",
        ["quote", []],
        ["quote", ["key", "value"]],
        ["quote", ["gt?", ["var-ref", "value"], 3]]
      ], {"a":1, "b":2, "c":3, "d": 4, "e": 5, "f": 6}]
    )air");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().toJson(), "{\"d\":4,\"e\":5,\"f\":6}");
}

TEST_F(GreenspunTest, filter_dict_keys) {
  auto program = arangodb::velocypack::Parser::fromJson(R"air(
      ["filter", ["lambda",
        ["quote", []],
        ["quote", ["key", "value"]],
        ["quote", ["eq?", ["var-ref", "key"], "d"]]
      ], {"a":1, "b":2, "c":3, "d": 4, "e": 5, "f": 6}]
    )air");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().toJson(), "{\"d\":4}");
}

TEST_F(GreenspunTest, dict_huh) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict?", {"a":"b"}]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isTrue());

  program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict?", 12]
    )aql");

  result.clear();
  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isFalse()) << result.slice().toJson();
}

TEST_F(GreenspunTest, dict_empty) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().toJson(), "{}");
}

TEST_F(GreenspunTest, dict_single_pair) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict", ["quote", ["a", 5]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().toJson(), R"json({"a":5})json");
}

TEST_F(GreenspunTest, dict_two_pairs) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict", ["quote", ["a", 5]], ["quote", ["b", "abc"]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().toJson(), R"json({"a":5,"b":"abc"})json");
}

TEST_F(GreenspunTest, dict_keys_empty) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-keys", {}]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().toJson(), "[]");
}

TEST_F(GreenspunTest, dict_keys_object) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-keys", {"a": 1, "b": 2, "c": 3}]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isArray());
  ASSERT_EQ(result.slice().length(), 3);
  ASSERT_EQ(result.slice().toJson(), "[\"a\",\"b\",\"c\"]");
}

TEST_F(GreenspunTest, dict_keys_error) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-keys"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, reduce_list) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
        ["reduce",
          ["list", 1, 2, 3],
          ["lambda",
            ["quote", []],
            ["quote", ["key", "value", "accum"]],
            ["quote",
              ["seq",
                ["+", ["var-ref", "value"], ["var-ref", "accum"] ]
              ]
            ]
          ]
        ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  // Definition does not allow reduce without initial accumulator being set
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, reduce_list_init) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
        ["reduce",
          ["list", 1, 2, 3],
          ["lambda",
            ["quote", []],
            ["quote", ["key", "value", "accum" ]],
            ["quote",
              ["seq",
                ["+", ["var-ref", "value"], ["var-ref", "accum"] ]
              ]
            ]
          ],
          100
        ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 106);
}

TEST_F(GreenspunTest, reduce_list_init_list) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
        ["reduce",
          ["list", 1, 2, 3],
          ["lambda",
            ["quote", []],
            ["quote", ["key", "value", "accum" ]],
            ["quote",
              [
                "list-set",
                ["var-ref", "accum"],
                ["var-ref", "key"],
                ["+", ["var-ref", "value"], ["list-ref", ["var-ref", "accum"], ["var-ref", "key"]] ]
              ]
            ]
          ],
          ["list", 1, 2, 3]
        ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(result.slice().toJson(), "[2,4,6]");
}

/* Objects */

TEST_F(GreenspunTest, reduce_dict) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
        ["reduce",
          {"a": 1, "b": 2, "c": 3},
          ["lambda",
            ["quote", []],
            ["quote", ["key", "value", "accum"]],
            ["quote",
              ["seq",
                ["+", ["var-ref", "value"], ["var-ref", "accum"] ]
              ]
            ]
          ]
        ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, reduce_dict_init) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
        ["reduce",
          {"a": 1, "b": 2, "c": 3},
          ["lambda",
            ["quote", []],
            ["quote", ["key", "value", "accum"]],
            ["quote",
              ["seq",
                ["+", ["var-ref", "value"], ["var-ref", "accum"] ]
              ]
            ]
          ],
          100
        ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 106);
}

/*
 * Input:  {"a": 1, "b": 2, "c": 3}
 * Accum:  {"a": 1, "b": 2, "c": 3, "d": 4}
 * Result: {"a": 2, "b": 4, "c": 6, "d": 4}
 */

TEST_F(GreenspunTest, reduce_dict_init_list) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
        ["reduce",
          {"a": 1, "b": 2, "c": 3},
          ["lambda",
            ["quote", []],
            ["quote", ["key", "value", "accum" ]],
            ["seq",
              ["quote",
                [
                  "attrib-set",
                  ["var-ref", "accum"],
                  ["var-ref", "key"],
                  ["+", ["var-ref", "value"], ["attrib-ref", ["var-ref", "accum"], ["var-ref", "key"]] ]
                ]
              ]
            ]
          ],
          {"a": 1, "b": 2, "c": 3, "d": 4}
        ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(result.slice().toJson(), R"json({"a":2,"b":4,"c":6,"d":4})json");
}

TEST_F(GreenspunTest, reduce_dict_init_dicts) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
        ["reduce",
          {"a": 1, "b": 2, "c": 3, "e": 5},
          ["lambda",
            ["quote", []],
            ["quote", ["key", "value", "accum" ]],
            ["seq",
              ["quote",
                [
                  "attrib-set",
                  ["var-ref", "accum"],
                  ["var-ref", "key"],
                  ["+", ["var-ref", "value"], ["attrib-ref-or", ["var-ref", "accum"], ["var-ref", "key"], 0] ]
                ]
              ]
            ]
          ],
          {"a": 1, "b": 2, "c": 3, "d": 4}
        ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(result.slice().toJson(), R"json({"a":2,"b":4,"c":6,"d":4,"e":5})json");
}

TEST_F(GreenspunTest, sort) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["sort", "lt?", ["list", 3, 1, 2]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().toJson(), "[1,2,3]");
}
/*
TEST_F(GreenspunTest, str_empty) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["str"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().copyString(), "");
}

TEST_F(GreenspunTest, str_single) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["str", "yello"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().copyString(), "yello");
}

TEST_F(GreenspunTest, str_multi) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["str", "yello", "world"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().copyString(), "yelloworld");
}

TEST_F(GreenspunTest, for_each) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["str", "yello", "world"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().copyString(), "yelloworld");
}*/

// TODO error testing for str

TEST_F(GreenspunTest, dict_merge_left_empty) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict"], ["dict", ["quote", ["hello", "world"]]] ]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isObject());
  ASSERT_TRUE(result.slice().get("hello").isString());
  ASSERT_EQ(result.slice().get("hello").toString(), "world");
}

TEST_F(GreenspunTest, dict_merge_right_empty) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict", ["quote", ["hello", "world"]]], ["dict"]]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isObject());
  ASSERT_TRUE(result.slice().get("hello").isString());
  ASSERT_EQ(result.slice().get("hello").toString(), "world");
}

TEST_F(GreenspunTest, dict_merge_overwrite) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict", ["quote", ["hello", "world"]]], ["dict", ["quote", ["hello", "newWorld"]]]]
    )aql");

  Evaluate(m, program->slice(), result);
  ASSERT_TRUE(result.slice().isObject());
  ASSERT_TRUE(result.slice().get("hello").isString());
  ASSERT_EQ(result.slice().get("hello").toString(), "newWorld");
}

TEST_F(GreenspunTest, dict_merge_error_invalid_type) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict", ["quote", ["hello", "world"]]], "peter"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, attrib_set_key) {
  // ["attrib-set", dict, key, value]
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-set",
        ["dict", ["quote", ["hello", "world"]]],
        "hello", "newWorld"
      ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isObject());
  ASSERT_TRUE(result.slice().get("hello").isString());
  ASSERT_EQ(result.slice().get("hello").toString(), "newWorld");
}

TEST_F(GreenspunTest, attrib_set_path) {
  // ["attrib-set", dict, [path...], value]
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-set",
        {"first": {"second": "oldWorld"}},
        ["quote", ["first", "second"]], "newWorld"
      ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isObject());
  ASSERT_TRUE(result.slice().get("first").isObject());
  ASSERT_TRUE(result.slice().get("first").get("second").isString());
  ASSERT_EQ(result.slice().get("first").get("second").toString(), "newWorld");
}

TEST_F(GreenspunTest, attrib_set_path_array) {
  // ["attrib-set", dict, [path...], value]
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-set",
        {"first": {"second": "oldWorld"}},
        ["quote", ["first", "second"]], ["quote", ["new", "world"]]
      ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isObject());
  ASSERT_TRUE(result.slice().get("first").isObject());
  ASSERT_TRUE(result.slice().get("first").get("second").isArray());
  ASSERT_EQ(result.slice().get("first").get("second").length(), 2);
  ASSERT_TRUE(result.slice().get("first").get("second").at(0).copyString() ==
              "new");
  ASSERT_TRUE(result.slice().get("first").get("second").at(1).copyString() ==
              "world");
}

TEST_F(GreenspunTest, min_empty) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isNull());
}

TEST_F(GreenspunTest, min_one) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min", 1]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 1);
}

TEST_F(GreenspunTest, min_two) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min", 1, 2]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 1);
}

TEST_F(GreenspunTest, min_two_reversed) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min", 2, 1]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 1);
}
TEST_F(GreenspunTest, min_three) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min", 2, 1, 3]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 1);
}

TEST_F(GreenspunTest, min_error) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min", 1, "foo"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, array_ref_valid_index) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-ref", ["quote", [1, 2, 3, 4]], 0]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isNumber());
  ASSERT_EQ(result.slice().getNumericValue<uint64_t>(), 1);
}

TEST_F(GreenspunTest, array_ref_invalid_index) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-ref", ["quote", [1, 2, 3, 4]], 6]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, array_ref_error_no_array) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-ref", "aString", 1]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, array_ref_error_bad_index) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-ref", ["quote", [1, 2, 3, 4]], "notAValidIndex"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, array_ref_index_negative) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-ref", ["quote", [1, 2, 3, 4]], -1]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, array_set_valid_index) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-set", ["quote", [1, 2, 3, 4]], 0, "newValue"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isArray());
  ASSERT_EQ(result.slice().at(0).copyString(), "newValue");
}

TEST_F(GreenspunTest, array_set_invalid_index) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-set", ["quote", [1, 2, 3, 4]], 6, 10]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, array_error_no_array) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-set", "aString", 1, "peter"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, array_error_bad_index) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-set", ["quote", [1, 2, 3, 4]], "notAValidIndex", "hehe"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, apply_sum) {
  // ["attrib-set", dict, key, value]
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", "+", ["quote", [1, 2, 3]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 6);
}

TEST_F(GreenspunTest, apply_unknown) {
  // ["attrib-set", dict, key, value]
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", "function-not-found", ["quote", [1, 2, 3]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, apply_invalid_type) {
  // ["attrib-set", dict, key, value]
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", 12, ["quote", [1, 2, 3]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, apply_no_args) {
  // ["attrib-set", dict, key, value]
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", "+", "string"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, apply_lambda) {
  // ["attrib-set", dict, key, value]
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", ["lambda", ["quote", []], ["quote", ["x"]], ["quote", ["var-ref", "x"]]], ["quote", [2]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 2);
}

TEST_F(GreenspunTest, apply_no_reevaluate_parameters) {
  // ["attrib-set", dict, key, value]
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", ["lambda", ["quote", []], ["quote", ["x"]], 2], ["quote", [["error"]]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumericValue<double>(), 2);
}

TEST_F(GreenspunTest, quasi_quote_empty) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-empty?", ["quasi-quote", []]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isTrue());
}

TEST_F(GreenspunTest, quasi_quote_one) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-length", ["quasi-quote", [1]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumber<double>(), 1);
}

TEST_F(GreenspunTest, quasi_quote_unquote) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote", ["unquote", ["+", 1, 2]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumber<double>(), 3);
}

TEST_F(GreenspunTest, quasi_quote_unquote_multiple) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote", ["unquote", ["+", 1, 2], 5]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, quasi_quote_unquote_no_params) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote", ["unquote"]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  ASSERT_TRUE(res.fail());
}

TEST_F(GreenspunTest, quasi_quote_unquote_quasi_quote) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote",
        ["unquote",
          ["array-length",
            ["quasi-quote",
              [["unquote",
                ["+", 1, 2]
              ],
              2]
            ]
          ]
        ]
      ]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().getNumber<double>(), 2);
}

TEST_F(GreenspunTest, quasi_quote_unquote_splice) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote", [["foo"], ["unquote", ["list", 1, 2]], ["unquote-splice", ["list", 1, 2]]]]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_EQ(result.slice().toJson(), R"=([["foo"],[1,2],1,2])=");
}

TEST_F(GreenspunTest, rand) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["rand"]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isNumber<double>());
  ASSERT_TRUE(result.slice().getDouble() <= 1.0);
  ASSERT_TRUE(result.slice().getDouble() >= 0.0);
}

TEST_F(GreenspunTest, rand_range) {
  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["rand-range", 5, 9]
    )aql");

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isNumber<double>());
  ASSERT_TRUE(result.slice().getDouble() <= 9.0);
  ASSERT_TRUE(result.slice().getDouble() >= 5.0);
}

#ifdef AIR_PRIMITIVE_TESTS_MAIN
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
