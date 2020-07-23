#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "./third_party/catch.hpp"

#include <iostream>

#include "Pregel/Algos/VertexAccumulators/Greenspun/Interpreter.h"
#include "Pregel/Algos/VertexAccumulators/Greenspun/Primitives.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

namespace arangodb::basics::VelocyPackHelper {

int compare(arangodb::velocypack::Slice, arangodb::velocypack::Slice, bool,
            arangodb::velocypack::Options const*,
            arangodb::velocypack::Slice const*, arangodb::velocypack::Slice const*) {
  std::cerr << "WARNING! YOU ARE ABOUT TO SHOOT YOURSELF IN THE FOOT!" << std::endl;
  return 0;
}

}  // namespace arangodb::basics::VelocyPackHelper

struct MyEvalContext : PrimEvalContext {
  std::string const& getThisId() const override { std::abort(); }

  void getAccumulatorValue(std::string_view id, VPackBuilder& result) const override {
    std::abort();
  }

  void updateAccumulator(std::string_view accumId, std::string_view vertexId,
                         VPackSlice value) override {
    std::abort();
  }

  void setAccumulator(std::string_view accumId, VPackSlice value) override {
    std::abort();
  }

  EvalResult enumerateEdges(std::function<EvalResult(VPackSlice edge)> cb) const override {
    std::abort();
  }
};

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
    REQUIRE(2 == result.slice().getInt());
  }

  /*SECTION( "Test basic double addition" ) {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["+", 1.1, 2.1]
    )aql");

    Evaluate(ctx, program->slice(), result);

    REQUIRE( 3.2 == result.slice().getDouble());
  }*/
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
    REQUIRE(0 == result.slice().getInt());
  }

  SECTION("Test negative int result value") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 2, 4]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(-2 == result.slice().getInt());
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
    REQUIRE(4 == result.slice().getInt());
  }

  SECTION("Test int zero multiplication") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["*", 2, 0]
    )aql");

    Evaluate(ctx, program->slice(), result);
    REQUIRE(0 == result.slice().getInt());
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
    REQUIRE(1 == result.slice().getInt());
  }

  SECTION("Test invalid int division") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
    auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["/", 2, 0]
    )aql");

    REQUIRE_THROWS(Evaluate(ctx, program->slice(), result));
  }
}

TEST_CASE("Test [list] primitive", "[list]") {}
TEST_CASE("Test [eq?] primitive", "[equals]") {}
TEST_CASE("Test [varref] primitive", "[varref]") {}
TEST_CASE("Test [attrib] primitive", "[attrib]") {}
TEST_CASE("Test [this] primitive", "[this]") {}
TEST_CASE("Test [accumref] primitive", "[accumref]") {}
TEST_CASE("Test [update] primitive", "[update]") {}
TEST_CASE("Test [set] primitive", "[set]") {}
TEST_CASE("Test [for] primitive", "[for]") {}
TEST_CASE("Test [cat] primitive", "[cat]") {}

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
    REQUIRE("1" == result.slice().toString());
  }
}