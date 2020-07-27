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
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test basic int addition") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["+", 1, 1]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(2 == result.slice().getDouble());
  }

  SECTION( "Test basic double addition" ) {
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
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test basic int subtraction") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 1, 1]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(0 == result.slice().getDouble());
  }

  SECTION("Test basic double subtraction") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 4.4, 1.2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    // TODO: also do more precise double comparison here
    REQUIRE(3.2 == result.slice().getDouble());
  }

  SECTION("Test negative int result value") {
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
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test basic int multiplication") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["*", 2, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(4 == result.slice().getDouble());
  }

  SECTION("Test int zero multiplication") {
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
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test basic int division") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["/", 2, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(1 == result.slice().getDouble());
  }

  SECTION("Test invalid int division") {
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

TEST_CASE("Test [not] primitive - unary", "[not]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test true boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["not", true]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test false boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["not", false]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }
}

TEST_CASE("Test [false?] primitive", "[false?]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test true boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["false?", true]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test false boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["false?", false]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }
}

TEST_CASE("Test [true?] primitive", "[true?]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test true boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["true?", true]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test false boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["true?", false]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }
}

/*
 * Comparison operators
 */

TEST_CASE("Test [eq?] primitive", "[equals]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test equality with ints") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 2, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test non equality with ints") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 3, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test equality with doubles") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 2.2, 2.2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test non equality with doubles") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 2.4, 2.2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test equality with bools") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", true, true]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test non equality with bools") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", true, false]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }
  SECTION("Test equality with string") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", "hello", "hello"]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test non equality with string") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", "hello", "world"]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }
}

TEST_CASE("Test [gt?] primitive", "[greater]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test equality with int greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 2, 1]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with int lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 1, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test equality with double greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 2.4, 1.3]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with double lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 1.1, 2.3]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test equality with true first") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", true, false]
    )aql");

    EvalResult res = Evaluate(ctx, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test equality with true first") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", false, true]
    )aql");

    EvalResult res = Evaluate(ctx, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test equality equal bool true") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", true, true]
    )aql");

    EvalResult res = Evaluate(ctx, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test equality equal bool false") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", false, false]
    )aql");

    EvalResult res = Evaluate(ctx, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test equality strings") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", "astring", "bstring"]
    )aql");

    EvalResult res = Evaluate(ctx, program->slice(), result);
    REQUIRE(res.fail());
  }
}


TEST_CASE("Test [ge?] primitive", "[greater equal]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test equality with int greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2, 1]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with int greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with int lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 1, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test equality with double greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2.4, 1.3]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with double greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2.4, 2.4]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with double lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 1.1, 2.3]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test equality with true first") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", true, false]
    )aql");

    EvalResult res = Evaluate(ctx, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test equality strings") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", "astring", "bstring"]
    )aql");

    EvalResult res = Evaluate(ctx, program->slice(), result);
    REQUIRE(res.fail());
  }
}

TEST_CASE("Test [le?] primitive", "[less equal]") {}
TEST_CASE("Test [lt?] primitive", "[less]") {}
TEST_CASE("Test [ne?] primitive", "[not equal]") {
  InitInterpreter();
  MyEvalContext ctx;
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test equality with ints") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 2, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test non equality with ints") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 3, 2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with doubles") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 2.2, 2.2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test non equality with doubles") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 2.4, 2.2]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with bools") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", true, true]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test non equality with bools") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", true, false]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }
  SECTION("Test equality with string") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", "hello", "hello"]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test non equality with string") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", "hello", "world"]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }
}

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
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test int to string conversion") {
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

