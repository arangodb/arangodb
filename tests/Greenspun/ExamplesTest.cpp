#include "gtest/gtest.h"

#include "Pregel/Algos/AIR/Greenspun/Interpreter.h"
#include "Pregel/Algos/AIR/Greenspun/Primitives.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include <string>
#include <iostream>

using namespace arangodb::greenspun;
using namespace std::string_literals;

struct GreenspunExamplesTest : public ::testing::Test {
  Machine m;
  VPackBuilder result;

  GreenspunExamplesTest() { InitMachine(m); }
};

#if defined(__clang_analyzer__) || defined(__JETBRAINS__IDE__)
// Disable warning
// "Initialization of [...] with static storage duration may
//  throw an exception that cannot be caught".
#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"
#endif

namespace snippet {

  auto const yCombinator = R"air(
    ["lambda", ["list"], ["quote", ["h"]], ["quote",
      [["lambda", ["quote", ["h"]], ["quote", ["x"]], ["quote",
        [["var-ref", "h"], ["lambda", ["list", "x"], ["quote", ["a"]], ["quote",
          [[["var-ref", "x"], ["var-ref", "x"]], ["var-ref", "a"]]]]]]],
      ["lambda", ["quote", ["h"]], ["quote", ["x"]], ["quote",
        [["var-ref", "h"], ["lambda", ["list", "x"], ["quote", ["a"]], ["quote",
          [[["var-ref", "x"], ["var-ref", "x"]], ["var-ref", "a"]]]]]]]]
    ]]
  )air"s;

  auto const join = R"air(
    ["lambda", ["list"], ["quote", ["sep", "arr"]], ["quote",
      ["apply", "string-cat",
        ["map", ["lambda", ["list", "sep"], ["list", "i", "x"], ["quote",
          ["if",
            [["lt?", 0, ["var-ref", "i"]], ["string-cat", ["var-ref", "sep"], ["var-ref", "x"]]],
            [true, ["var-ref", "x"]]
          ]]],
          ["var-ref", "arr"]]
      ]
    ]]
  )air"s;

  // needs "join" to be available; is "recursive" via the Y combinator,
  // recursive call via "self" argument
  // does not work perfectly: doubles are rounded to integers, characters in
  // strings (like a double quote) aren't escaped, and objects aren't handled.
  // But arrays work, also recursively, and this is to showcase that that works.
  auto const naiveToJson = R"air(
    ["lambda", ["quote", ["join"]], ["quote", ["self"]], ["quote",
        ["lambda", ["quote", ["join", "self"]], ["quote", ["obj"]], ["quote",
            ["if",
              [["string?", ["var-ref", "obj"]], ["string-cat", "\"", ["var-ref", "obj"], "\""]],
              [["null?", ["var-ref", "obj"]], ["string-cat", "null"]],
              [["bool?", ["var-ref", "obj"]], ["if", [["true?", ["var-ref", "obj"]], ["string-cat", "true"]], [["false?", ["var-ref", "obj"]], ["string-cat", "false"]]]],
              [["number?", ["var-ref", "obj"]], ["int-to-str", ["var-ref", "obj"]]],
              [["list?", ["var-ref", "obj"]], ["string-cat", "[", [["var-ref", "join"], ",",
                ["map", ["lambda", ["quote", ["self"]], ["quote", ["i", "x"]], ["quote",
                  [["var-ref", "self"], ["var-ref", "x"]]
                ]], ["var-ref", "obj"]]
              ], "]"]],
              [["dict?", ["var-ref", "obj"]], ["error", "dicts aren't implemented"]],
              [true, ["error", "unhandled value"]]]
        ]]
    ]]
  )air"s;
}

TEST_F(GreenspunExamplesTest, join_empty) {
  auto program = arangodb::velocypack::Parser::fromJson(R"air(
    ["let", [
      ["join", )air"s + snippet::join + R"air(]],
      [["var-ref", "join"], ", ", ["list"]]
    ]
  )air"s);

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().stringView(), ""s);
}

TEST_F(GreenspunExamplesTest, join_one) {
  auto program = arangodb::velocypack::Parser::fromJson(R"air(
    ["let", [
      ["join", )air"s + snippet::join + R"air(]],
      [["var-ref", "join"], ", ", ["list", "foo"]]
    ]
  )air"s);

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().stringView(), "foo"s);
}

TEST_F(GreenspunExamplesTest, join_two) {
  auto program = arangodb::velocypack::Parser::fromJson(R"air(
    ["let", [
      ["join", )air"s + snippet::join + R"air(]],
      [["var-ref", "join"], ", ", ["list", "foo", "bar"]]
    ]
  )air"s);

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().stringView(), "foo, bar"s);
}

TEST_F(GreenspunExamplesTest, join_three) {
  auto program = arangodb::velocypack::Parser::fromJson(R"air(
    ["let", [
      ["join", )air"s + snippet::join + R"air(]],
      [["var-ref", "join"], ", ", ["list", "foo", "bar", "baz"]]
    ]
  )air"s);

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().stringView(), "foo, bar, baz"s);
}

TEST_F(GreenspunExamplesTest, y_combinator_naive_to_json) {
  // Test a simple recursive program via the Y combinator
  auto program = arangodb::velocypack::Parser::fromJson(R"air(
    ["let", [["join", )air"s + snippet::join + R"air(]],
      ["let", [
          ["Y", )air"s + snippet::yCombinator + R"air(],
          ["toJsonBase", )air"s + snippet::naiveToJson + R"air(]],
        ["let", [["toJson", [["var-ref", "Y"], ["var-ref", "toJsonBase"]]]],
          [["var-ref", "toJson"], ["list", 6, ["list", null, false], ["list", "foo", ["list", ["list"]]], "bar"]]]
    ]]
  )air"s);
  auto const expected = R"json([6,[null,false],["foo",[[]]],"bar"])json"s;

  auto res = Evaluate(m, program->slice(), result);
  if (res.fail()) {
    FAIL() << res.error().toString();
  }
  ASSERT_TRUE(result.slice().isString());
  ASSERT_EQ(result.slice().stringView(), expected);
}

#if defined(__clang_analyzer__) || defined(__JETBRAINS__IDE__)
#pragma clang diagnostic pop
#endif
