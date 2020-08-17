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

TEST_CASE("Test [attrib] primitive", "[attrib]") {}

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
TEST_CASE("Test [var-set!] primitive", "[var-set!]") {
  REQUIRE(false);
}

TEST_CASE("Test [bind-ref] primitive", "[bind-ref]") {
  REQUIRE(false);
}

TEST_CASE("Test [for-each] primitive", "[for-each]") {
  REQUIRE(false);
}


// TODO: this is not a language primitive but part of `VertexComputation`
TEST_CASE("Test [accum-ref] primitive", "[accumref]") {}
TEST_CASE("Test [this] primitive", "[this]") {}
TEST_CASE("Test [send-to-accum] primitive", "[send-to-accum]") {}
TEST_CASE("Test [send-to-all-neighbours] primitive", "[send-to-all-neighbours]") {}




TEST_CASE("Test [global-superstep] primitive", "[global-superstep]") {}

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
