#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "./third_party/catch.hpp"

#include <iostream>

#include "Pregel/Algos/VertexAccumulators/Greenspun/Interpreter.h"
#include "Pregel/Algos/VertexAccumulators/Greenspun/Primitives.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"
#include "./structs/EvalContext.h"

/*
 * Calculation operators
 */

TEST_CASE("Test [+] primitive", "[addition]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;

  SECTION("Test basic int addition") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["+", 1, 1]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(2 == result.slice().getDouble());
  }

  SECTION( "Test basic double addition" ) {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["+", 1.1, 2.1]
    )aql");

    Evaluate(ctx, program->slice(), result);

    REQUIRE( 3.2 == result.slice().getDouble());
  }
}

TEST_CASE("Test [-] primitive", "[subtraction]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;

  SECTION("Test basic int subtraction") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 1, 1]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(0 == result.slice().getDouble());
  }

  SECTION("Test basic double subtraction") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 4.4, 1.2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    // TODO: also do more precise double comparison here
    REQUIRE(3.2 == result.slice().getDouble());
  }

  SECTION("Test negative int result value") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 2, 4]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(-2 == result.slice().getDouble());
  }
}

TEST_CASE("Test [*] primitive", "[multiplication]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;

  SECTION("Test basic int multiplication") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["*", 2, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(4 == result.slice().getDouble());
  }

  SECTION("Test int zero multiplication") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["*", 2, 0]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(0 == result.slice().getDouble());
  }
}

TEST_CASE("Test [/] primitive", "[division]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;

  SECTION("Test basic int division") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["/", 2, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(1 == result.slice().getDouble());
  }

  SECTION("Test invalid int division") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["/", 2, 0]
    )aql");

    EvalResult res = Evaluate(ctx, program->slice(), result);
    REQUIRE(res.fail());
  }
}

/*
 * Logical operators
 */

TEST_CASE("Test [not] primitive - unary", "[not]") {}
TEST_CASE("Test [false?] primitive", "[false?]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;

  SECTION("Test true boolean") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["false?", true]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }
}
TEST_CASE("Test [true?] primitive", "[true?]") {}

/*
 * Comparison operators
 */

TEST_CASE("Test [eq?] primitive", "[equals]") {}
TEST_CASE("Test [gt?] primitive", "[greater]") {}
TEST_CASE("Test [ge?] primitive", "[greater equal]") {}
TEST_CASE("Test [le?] primitive", "[less equal]") {}
TEST_CASE("Test [lt?] primitive", "[less]") {}
TEST_CASE("Test [ne?] primitive", "[not equal]") {}

/*
 * Debug operators
 */

TEST_CASE("Test [print] primitive", "[print]") {

}

TEST_CASE("Test [list-cat] primitive", "[list-cat]") {}
TEST_CASE("Test [string-cat] primitive", "[string-cat]") {}
TEST_CASE("Test [int-to-str] primitive", "[int-to-str]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;

  SECTION("Test int to string conversion") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["int-to-str", 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE("2" == result.slice().toString());
  }
}

/*
 * Access operators
 */

TEST_CASE("Test [attrib] primitive", "[attrib]") {}
TEST_CASE("Test [var-ref] primitive", "[var-ref]") {}
TEST_CASE("Test [bind-ref] primitive", "[bind-ref]") {}
TEST_CASE("Test [accum-ref] primitive", "[accumref]") {}

TEST_CASE("Test [this] primitive", "[this]") {}
TEST_CASE("Test [update] primitive", "[update]") {}
TEST_CASE("Test [set] primitive", "[set]") {}
TEST_CASE("Test [for] primitive", "[for]") {}
TEST_CASE("Test [global-superstep] primitive", "[global-superstep]") {}

