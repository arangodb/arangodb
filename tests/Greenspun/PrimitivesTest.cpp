#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "./third_party/catch.hpp"

#include <iostream>

#include "Pregel/Algos/AIR/Greenspun/Interpreter.h"
#include "Pregel/Algos/AIR/Greenspun/Primitives.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

/*
 * Calculation operators
 */

using namespace arangodb::greenspun;

TEST_CASE("Test [dict-x-tract] primitive", "[dict-x-tract]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("Test dict-x-tract") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-x-tract", {"foo":1, "bar":3}, "foo"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if(res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isObject());
    REQUIRE(result.slice().get("foo").getNumericValue<double>() == 1);
    REQUIRE(result.slice().get("bar").isNone());
  }

  SECTION("Test dict-x-tract") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-x-tract", {"foo":1, "bar":3}, "baz"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test dict-x-tract-x") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-x-tract-x", {"foo":1, "bar":3}, "foo"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if(res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isObject());
    REQUIRE(result.slice().get("foo").getNumericValue<double>() == 1);
    REQUIRE(result.slice().get("bar").isNone());
  }

  SECTION("Test dict-x-tract") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-x-tract-x", {"foo":1, "bar":3}, "baz"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.ok());
    REQUIRE(result.slice().isEmptyObject());
  }
}

TEST_CASE("Test [+] primitive", "[addition]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test basic int addition") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["+", 1, 1]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(2 == result.slice().getDouble());
  }

  SECTION("Test basic double addition") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["+", 1.1, 2.1]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(3.2 == result.slice().getDouble());
  }
}

TEST_CASE("Test [-] primitive", "[subtraction]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test basic int subtraction") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 1, 1]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(0 == result.slice().getDouble());
  }

  SECTION("Test basic double subtraction") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 4.4, 1.2]
    )aql");

    Evaluate(m, program->slice(), result);
    // TODO: also do more precise double comparison here
    REQUIRE(3.2 == result.slice().getDouble());
  }

  SECTION("Test negative int result value") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["-", 2, 4]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(-2 == result.slice().getDouble());
  }
}

TEST_CASE("Test [*] primitive", "[multiplication]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test basic int multiplication") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["*", 2, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(4 == result.slice().getDouble());
  }

  SECTION("Test int zero multiplication") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["*", 2, 0]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(0 == result.slice().getDouble());
  }
}

TEST_CASE("Test [/] primitive", "[division]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test basic int division") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["/", 2, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(1 == result.slice().getDouble());
  }

  SECTION("Test invalid int division") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["/", 2, 0]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

/*
 * Logical operators
 */

TEST_CASE("Test [not] primitive - unary", "[not]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test true boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["not", true]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test false boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["not", false]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }
}

TEST_CASE("Test [false?] primitive", "[false?]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test true boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["false?", true]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test false boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["false?", false]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }
}

TEST_CASE("Test [true?] primitive", "[true?]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test true boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["true?", true]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test false boolean") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["true?", false]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }
}

/*
 * Comparison operators
 */

TEST_CASE("Test [eq?] primitive", "[equals]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test equality with ints") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 2, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test non equality with ints") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 3, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test equality with doubles") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 2.2, 2.2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test non equality with doubles") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", 2.4, 2.2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test equality with bools") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", true, true]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test non equality with bools") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", true, false]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }
  SECTION("Test equality with string") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", "hello", "hello"]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with string") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["eq?", "hello", "world"]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }
}

TEST_CASE("Test [gt?] primitive", "[greater]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test greater than with int greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 2, 1]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test greater than with int lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 1, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test greater than with double greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 2.4, 1.3]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test greater than with double lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", 1.1, 2.3]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test greater than with true first") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", true, false]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test greater than with true first") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", false, true]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test greater than equal bool true") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", true, true]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test greater than equal bool false") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", false, false]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test greater than strings") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["gt?", "astring", "bstring"]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

TEST_CASE("Test [ge?] primitive", "[greater equal]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test greater equal with int greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2, 1]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test greater equal with int greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test greater equal with int lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 1, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test greater equal with double greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2.4, 1.3]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test greater equal with double greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 2.4, 2.4]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test greater equal with double lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", 1.1, 2.3]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test greater equal with true first") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", true, false]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test greater equal strings") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ge?", "astring", "bstring"]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

TEST_CASE("Test [le?] primitive", "[less equal]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test less equal with int greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 2, 1]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test less equal with int greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 2, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test less equal with int lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 1, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test less equal with double greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 2.4, 1.3]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test less equal with double greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 2.4, 2.4]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test less equal with double lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", 1.1, 2.3]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test less equal with true first") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", true, false]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test less equal strings") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["le?", "astring", "bstring"]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

TEST_CASE("Test [lt?] primitive", "[less]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test less than with int greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 2, 1]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test less than with int greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 2, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test less than with int lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 1, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test less than with double greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 2.4, 1.3]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test less than with double greater") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 2.4, 2.4]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test less than with double lower") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", 1.1, 2.3]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test less than with true first") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", true, false]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Test less than strings") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["lt?", "astring", "bstring"]
    )aql");

    EvalResult res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

TEST_CASE("Test [ne?] primitive", "[not equal]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test equality with ints") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 2, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test non equality with ints") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 3, 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with doubles") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 2.2, 2.2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test non equality with doubles") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", 2.4, 2.2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }

  SECTION("Test equality with bools") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", true, true]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test non equality with bools") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", true, false]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }
  SECTION("Test equality with string") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", "hello", "hello"]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(false == result.slice().getBoolean());
  }

  SECTION("Test non equality with string") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["ne?", "hello", "world"]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(true == result.slice().getBoolean());
  }
}

/*
 * Debug operators
 */

TEST_CASE("Test [print] primitive", "[print]") {
  // idk if that one here is necessary. Only a method for debug purpose.
}

TEST_CASE("Test [list-cat] primitive", "[list-cat]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test list cat basic, single param") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["list-cat", ["quote", 1, 2, 3]]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(result.slice().isArray());
    REQUIRE(result.slice().length() == 3);
    for (size_t i = 0; i < 3; i++) {
      REQUIRE(result.slice().at(i).getInt() == (i + 1));
    }
  }

  SECTION("Test list cat basic, single param") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["list-cat", ["quote", 1, 2, 3], ["quote", 4, 5]]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(result.slice().isArray());
    REQUIRE(result.slice().length() == 5);
    for (size_t i = 0; i < 5; i++) {
      REQUIRE(result.slice().at(i).getInt() == (i + 1));
    }
  }
}

TEST_CASE("Test [string-cat] primitive", "[string-cat]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test string concat single param") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["string-cat", "hello"]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(result.slice().isString());
    REQUIRE(result.slice().toString() == "hello");
  }

  SECTION("Test string concat single param") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["string-cat", "hello", "world"]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(result.slice().isString());
    REQUIRE(result.slice().toString() == "helloworld");
  }
}

TEST_CASE("Test [int-to-str] primitive", "[int-to-str]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  SECTION("Test int to string conversion") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["int-to-str", 2]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(result.slice().isString());
    REQUIRE("2" == result.slice().toString());
  }
}

/*
 * Access operators
 */

TEST_CASE("Test [attrib-ref] primitive", "[attrib-ref]") { REQUIRE(false); }

TEST_CASE("Test [var-ref] primitive", "[var-ref]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;
  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  // TODO: add variables to m (vertexData)
  SECTION("Test get non existing variable") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["var-ref", "peter"]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(result.slice().isNone());
  }
}

TEST_CASE("Test [var-set!] primitive", "[var-set!]") { REQUIRE(false); }

TEST_CASE("Test [bind-ref] primitive", "[bind-ref]") { REQUIRE(false); }

TEST_CASE("Test [for-each] primitive", "[for-each]") { REQUIRE(false); }

TEST_CASE("Test [lambda] primitive", "[lambda]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("constant lambda") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [["lambda", ["quote"], ["quote"], 12]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    INFO(result.slice().toJson());
    REQUIRE(result.slice().getNumericValue<double>() == 12);
  }

  SECTION("constant lambda with expression") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [["lambda", ["quote"], ["quote"], ["+", 10, 2]]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    INFO(result.slice().toJson());
    REQUIRE(result.slice().getNumericValue<double>() == 12);
  }

  SECTION("lambda single parameter") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [["lambda", ["quote"], ["quote", "x"], ["quote", "var-ref", "x"]], 12]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    INFO(result.slice().toJson());
    REQUIRE(result.slice().getNumericValue<double>() == 12);
  }

  SECTION("lambda multiple parameter") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [["lambda", ["quote"], ["quote", "a", "b"],
        ["quote", "+",
          ["var-ref", "a"],
          ["var-ref", "b"]]
      ], 10, 2]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    INFO(result.slice().toJson());
    REQUIRE(result.slice().getNumericValue<double>() == 12);
  }

  SECTION("lambda single capture") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql(12)aql");
    m.setVariable("a", v->slice());

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [
        ["lambda", ["quote", "a"], ["quote"], ["quote", "var-ref", "a"]]
      ]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    INFO(result.slice().toJson());
    REQUIRE(result.slice().getNumericValue<double>() == 12);
  }

  SECTION("lambda single capture, single param") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql(8)aql");
    m.setVariable("a", v->slice());

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [
        ["lambda", ["quote", "a"], ["quote", "b"], ["quote", "+", ["var-ref", "a"], ["var-ref", "b"]]],
        4
      ]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    INFO(result.slice().toJson());
    REQUIRE(result.slice().getNumericValue<double>() == 12);
  }

  SECTION("lambda does not see vars that are not captured") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql(8)aql");
    m.setVariable("a", v->slice());

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [
        ["lambda", ["quote"], ["quote"], ["quote", "var-ref", "a"]]
      ]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("lambda call evaluates parameter") {
    auto v = arangodb::velocypack::Parser::fromJson(R"aql(8)aql");
    m.setVariable("a", v->slice());

    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      [
        ["lambda", ["quote"], ["quote", "x"], ["quote", "var-ref", "x"]],
        ["+", 10, 2]
      ]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    INFO(result.slice().toJson());
    REQUIRE(result.slice().getNumericValue<double>() == 12);
  }
}

TEST_CASE("Test [let] primitive", "[let]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("no binding") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [], 12]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 12.);
  }

  SECTION("no binding, seq") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [], 8, 12]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 12.);
  }

  SECTION("simple binding") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [["a", 12]], ["var-ref", "a"]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 12.);
  }

  SECTION("simple binding, double naming") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [["a", 1], ["a", 12]], ["var-ref", "a"]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 12.);
  }

  SECTION("multiple binding") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [["a", 1], ["b", 11]], ["+", ["var-ref", "a"], ["var-ref", "b"]]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 12.);
  }

  SECTION("no params - error") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("no list - error") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", "foo"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("no pairs - error") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [[1, 2, 3]]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("no string name - error") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [[1, 2]]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("bad seq - error") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["let", [["foo", 2]], ["foo"]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

// TODO: HACK HACK HACK FIXME DO A COMPARE FUNCTION FOR OBJECTS
TEST_CASE("Test [dict] primitive", "[dict]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("no content") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().toJson() == "{}");
  }

  SECTION("one content") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict", ["quote", "a", 5]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().toJson() == R"json({"a":5})json");
  }

  SECTION("two content") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict", ["quote", "a", 5], ["quote", "b", "abc"]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().toJson() == R"json({"a":5,"b":"abc"})json");
  }
}

TEST_CASE("Test [dict-keys] primitive", "[dict-keys]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("empty dict") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-keys", {}]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().toJson() == "[]");
  }

  SECTION("dict with 3 tuples") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-keys", {"a": 1, "b": 2, "c": 3}]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isArray());
    REQUIRE(result.slice().length() == 3);
    REQUIRE(result.slice().toJson() == "[\"a\",\"b\",\"c\"]");
  }

  SECTION("no content") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-keys"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

TEST_CASE("Test [dict-keys] primitive", "[dict-keys]") {
  // TODO: to be implemented!
}

TEST_CASE("Test [str] primitive", "[str]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("no content") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["str"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isString());
    REQUIRE(result.slice().copyString() == "");
  }

  SECTION("one content") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["str", "yello"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isString());
    REQUIRE(result.slice().copyString() == "yello");
  }

  SECTION("two content") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["str", "yello", "world"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isString());
    REQUIRE(result.slice().copyString() == "yelloworld");
  }

  // TODO error testing
}

TEST_CASE("Test [dict-merge] primitive", "[dict-merge]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("Merge with empty (left) object") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict"], ["dict", ["quote", "hello", "world"]] ]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(result.slice().isObject());
    REQUIRE(result.slice().get("hello").isString());
    REQUIRE(result.slice().get("hello").toString() == "world");
  }

  SECTION("Merge with empty (right) object") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict", ["quote", "hello", "world"]], ["dict"]]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(result.slice().isObject());
    REQUIRE(result.slice().get("hello").isString());
    REQUIRE(result.slice().get("hello").toString() == "world");
  }

  SECTION("Merge with overwrite") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict", ["quote", "hello", "world"]], ["dict", ["quote", "hello", "newWorld"]]]
    )aql");

    Evaluate(m, program->slice(), result);
    REQUIRE(result.slice().isObject());
    REQUIRE(result.slice().get("hello").isString());
    REQUIRE(result.slice().get("hello").toString() == "newWorld");
  }

  SECTION("Merge with invalid type string") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict", ["quote", "hello", "world"]], "peter"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Merge with invalid type double") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict", ["quote", "hello", "world"]], "2.0"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Merge with invalid type bool") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict", ["quote", "hello", "world"]], true]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Merge with invalid type array") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["dict-merge", ["dict", ["quote", "hello", "world"]], [1,2,3]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

TEST_CASE("Test [attrib-set] primitive", "[attrib-set]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("Set string value with key") {
    // ["attrib-set", dict, key, value]
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-set",
        ["dict", ["quote", "hello", "world"]],
        "hello", "newWorld"
      ]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isObject());
    REQUIRE(result.slice().get("hello").isString());
    REQUIRE(result.slice().get("hello").toString() == "newWorld");
  }

  SECTION("Set string value with path (array)") {
    // ["attrib-set", dict, [path...], value]
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-set",
        {"first": {"second": "oldWorld"}},
        ["quote", "first", "second"], "newWorld"
      ]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isObject());
    REQUIRE(result.slice().get("first").isObject());
    REQUIRE(result.slice().get("first").get("second").isString());
    REQUIRE(result.slice().get("first").get("second").toString() == "newWorld");
  }

  SECTION("Set array value with path (array)") {
    // ["attrib-set", dict, [path...], value]
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["attrib-set",
        {"first": {"second": "oldWorld"}},
        ["quote", "first", "second"], ["quote", "new", "world"]
      ]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isObject());
    REQUIRE(result.slice().get("first").isObject());
    REQUIRE(result.slice().get("first").get("second").isArray());
    REQUIRE(result.slice().get("first").get("second").length() == 2);
    REQUIRE(result.slice().get("first").get("second").at(0).copyString() ==
            "new");
    REQUIRE(result.slice().get("first").get("second").at(1).copyString() ==
            "world");
  }
}

TEST_CASE("Test [min] primitive", "[min]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("empty min") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isNone());
  }

  SECTION("single min") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min", 1]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 1);
  }

  SECTION("double min") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min", 1, 2]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 1);
  }

  SECTION("double min 2.0") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min", 2, 1]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 1);
  }

  SECTION("triple min") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min", 2, 1, 3]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 1);
  }

  SECTION("min fail") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["min", 1, "foo"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

TEST_CASE("Test [array-ref] primitive", "[array-ref]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("get with valid index") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-ref", ["quote", 1, 2, 3, 4], 0]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isNumber());
    REQUIRE(result.slice().getNumericValue<uint64_t>() == 1);
  }

  SECTION("get with invalid index") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-ref", ["quote", 1, 2, 3, 4], 6]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("array not an array") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-ref", "aString", 1]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("index not a number") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-ref", ["quote", 1, 2, 3, 4], "notAValidIndex"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("index is a number but negative") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-ref", ["quote", 1, 2, 3, 4], -1]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

TEST_CASE("Test [array-set] primitive", "[array-set]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("get with valid index") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-set", ["quote", 1, 2, 3, 4], 0, "newValue"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isArray());
    REQUIRE(result.slice().at(0).copyString() == "newValue");
  }

  SECTION("get with invalid index") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-set", ["quote", 1, 2, 3, 4], 6, 10]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("array not an array") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-set", "aString", 1, "peter"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("index not a number") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-set", ["quote", 1, 2, 3, 4], "notAValidIndex", "hehe"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }
}

// TODO: this is not a language primitive but part of `VertexComputation`
TEST_CASE("Test [accum-ref] primitive", "[accumref]") {}
TEST_CASE("Test [this] primitive", "[this]") {}
TEST_CASE("Test [send-to-accum] primitive", "[send-to-accum]") {}
TEST_CASE("Test [send-to-all-neighbours] primitive",
          "[send-to-all-neighbours]") {}
TEST_CASE("Test [global-superstep] primitive", "[global-superstep]") {}

TEST_CASE("Test [apply] primitive", "[apply]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("Apply sum") {
    // ["attrib-set", dict, key, value]
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", "+", ["quote", 1, 2, 3]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 6);
  }

  SECTION("Apply sum, unknown function") {
    // ["attrib-set", dict, key, value]
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", "function-not-found", ["quote", 1, 2, 3]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Apply sum, no function type") {
    // ["attrib-set", dict, key, value]
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", 12, ["quote", 1, 2, 3]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Apply sum, no argument list") {
    // ["attrib-set", dict, key, value]
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", "+", "string"]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("Apply sum, lambda") {
    // ["attrib-set", dict, key, value]
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", ["lambda", ["quote"], ["quote", "x"], ["quote", "var-ref", "x"]], ["quote", 2]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 2);
  }

  SECTION("Apply not reevaluate parameter") {
    // ["attrib-set", dict, key, value]
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["apply", ["lambda", ["quote"], ["quote", "x"], 2], ["quote", ["error"]]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumericValue<double>() == 2);
  }
}

TEST_CASE("Test [quasi-quote] primitive", "[quasi-quote]") {
  Machine m;
  InitMachine(m);
  VPackBuilder result;

  SECTION("quasi-quote empty") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-empty?", ["quasi-quote"]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().isTrue());
  }

  SECTION("quasi-quote single") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["array-length", ["quasi-quote", 1]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().getNumber<double>() == 1);
  }

  SECTION("quasi-quote unquote") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote", ["unquote", ["+", 1, 2]]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().at(0).getNumber<double>() == 3);
  }

  SECTION("quasi-quote unquote multiple params") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote", ["unquote", ["+", 1, 2], 5]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("quasi-quote unquote multiple params") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote", ["unquote", ["+", 1, 2], 5]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("quasi-quote unquote no params") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote", ["unquote"]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    REQUIRE(res.fail());
  }

  SECTION("quasi-quote unquote quasi-quote") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote",
        ["unquote",
          ["array-length",
            ["quasi-quote",
              ["unquote",
                ["+", 1, 2]
              ],
              2
            ]
          ]
        ]
      ]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().at(0).getNumber<double>() == 2);
  }

  SECTION("quasi-quote unquote quasi-quote") {
    auto program = arangodb::velocypack::Parser::fromJson(R"aql(
      ["quasi-quote", ["foo"], ["unquote", ["list", 1, 2]], ["unquote-splice", ["list", 1, 2]]]
    )aql");

    auto res = Evaluate(m, program->slice(), result);
    if (res.fail()) {
      FAIL(res.error().toString());
    }
    REQUIRE(result.slice().toJson() == R"=([["foo"],[1,2],1,2])=");
  }
}
