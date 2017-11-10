// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>

#include "src/v8.h"

#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-generator.h"
#include "src/interpreter/interpreter.h"
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/interpreter/bytecode-expectations-printer.h"
#include "test/cctest/test-feedback-vector.h"

namespace v8 {
namespace internal {
namespace interpreter {

#define XSTR(A) #A
#define STR(A) XSTR(A)

#define UNIQUE_VAR() "var a" STR(__COUNTER__) " = 0;\n"

#define REPEAT_2(...) __VA_ARGS__ __VA_ARGS__
#define REPEAT_4(...) REPEAT_2(__VA_ARGS__) REPEAT_2(__VA_ARGS__)
#define REPEAT_8(...) REPEAT_4(__VA_ARGS__) REPEAT_4(__VA_ARGS__)
#define REPEAT_16(...) REPEAT_8(__VA_ARGS__) REPEAT_8(__VA_ARGS__)
#define REPEAT_32(...) REPEAT_16(__VA_ARGS__) REPEAT_16(__VA_ARGS__)
#define REPEAT_64(...) REPEAT_32(__VA_ARGS__) REPEAT_32(__VA_ARGS__)
#define REPEAT_128(...) REPEAT_64(__VA_ARGS__) REPEAT_64(__VA_ARGS__)
#define REPEAT_256(...) REPEAT_128(__VA_ARGS__) REPEAT_128(__VA_ARGS__)

#define REPEAT_127(...)  \
  REPEAT_64(__VA_ARGS__) \
  REPEAT_32(__VA_ARGS__) \
  REPEAT_16(__VA_ARGS__) \
  REPEAT_8(__VA_ARGS__)  \
  REPEAT_4(__VA_ARGS__)  \
  REPEAT_2(__VA_ARGS__)  \
  __VA_ARGS__

#define REPEAT_249(...)   \
  REPEAT_127(__VA_ARGS__) \
  REPEAT_64(__VA_ARGS__)  \
  REPEAT_32(__VA_ARGS__)  \
  REPEAT_16(__VA_ARGS__)  \
  REPEAT_8(__VA_ARGS__)   \
  REPEAT_2(__VA_ARGS__)

#define REPEAT_2_UNIQUE_VARS() UNIQUE_VAR() UNIQUE_VAR()
#define REPEAT_4_UNIQUE_VARS() REPEAT_2_UNIQUE_VARS() REPEAT_2_UNIQUE_VARS()
#define REPEAT_8_UNIQUE_VARS() REPEAT_4_UNIQUE_VARS() REPEAT_4_UNIQUE_VARS()
#define REPEAT_16_UNIQUE_VARS() REPEAT_8_UNIQUE_VARS() REPEAT_8_UNIQUE_VARS()
#define REPEAT_32_UNIQUE_VARS() REPEAT_16_UNIQUE_VARS() REPEAT_16_UNIQUE_VARS()
#define REPEAT_64_UNIQUE_VARS() REPEAT_32_UNIQUE_VARS() REPEAT_32_UNIQUE_VARS()
#define REPEAT_128_UNIQUE_VARS() REPEAT_64_UNIQUE_VARS() REPEAT_64_UNIQUE_VARS()

#define REPEAT_250_UNIQUE_VARS() \
  REPEAT_128_UNIQUE_VARS()       \
  REPEAT_64_UNIQUE_VARS()        \
  REPEAT_32_UNIQUE_VARS()        \
  REPEAT_16_UNIQUE_VARS()        \
  REPEAT_8_UNIQUE_VARS()         \
  UNIQUE_VAR()                   \
  UNIQUE_VAR()

static const char* kGoldenFileDirectory =
    "test/cctest/interpreter/bytecode_expectations/";

class InitializedIgnitionHandleScope : public InitializedHandleScope {
 public:
  InitializedIgnitionHandleScope() {
    i::FLAG_ignition = true;
    i::FLAG_always_opt = false;
    i::FLAG_allow_natives_syntax = true;
  }
};

void SkipGoldenFileHeader(std::istream& stream) {  // NOLINT
  std::string line;
  int separators_seen = 0;
  while (std::getline(stream, line)) {
    if (line == "---") separators_seen += 1;
    if (separators_seen == 2) return;
  }
}

std::string LoadGolden(const std::string& golden_filename) {
  std::ifstream expected_file((kGoldenFileDirectory + golden_filename).c_str());
  CHECK(expected_file.is_open());
  SkipGoldenFileHeader(expected_file);
  std::ostringstream expected_stream;
  // Restore the first separator, which was consumed by SkipGoldenFileHeader
  expected_stream << "---\n" << expected_file.rdbuf();
  return expected_stream.str();
}

template <size_t N>
std::string BuildActual(const BytecodeExpectationsPrinter& printer,
                        const char* (&snippet_list)[N],
                        const char* prologue = nullptr,
                        const char* epilogue = nullptr) {
  std::ostringstream actual_stream;
  for (const char* snippet : snippet_list) {
    std::string source_code;
    if (prologue) source_code += prologue;
    source_code += snippet;
    if (epilogue) source_code += epilogue;
    printer.PrintExpectation(actual_stream, source_code);
  }
  return actual_stream.str();
}

bool CompareTexts(const std::string& generated, const std::string& expected) {
  std::istringstream generated_stream(generated);
  std::istringstream expected_stream(expected);
  std::string generated_line;
  std::string expected_line;
  // Line number does not include golden file header.
  int line_number = 0;

  do {
    std::getline(generated_stream, generated_line);
    std::getline(expected_stream, expected_line);

    if (!generated_stream.good() && !expected_stream.good()) {
      return true;
    }

    if (!generated_stream.good()) {
      std::cerr << "Expected has extra lines after line " << line_number
                << "\n";
      std::cerr << "  Expected: '" << expected_line << "'\n";
      return false;
    } else if (!expected_stream.good()) {
      std::cerr << "Generated has extra lines after line " << line_number
                << "\n";
      std::cerr << "  Generated: '" << generated_line << "'\n";
      return false;
    }

    if (generated_line != expected_line) {
      std::cerr << "Inputs differ at line " << line_number << "\n";
      std::cerr << "  Generated: '" << generated_line << "'\n";
      std::cerr << "  Expected:  '" << expected_line << "'\n";
      return false;
    }
    line_number++;
  } while (true);
}

TEST(PrimitiveReturnStatements) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "",

      "return;\n",

      "return null;\n",

      "return true;\n",

      "return false;\n",

      "return 0;\n",

      "return +1;\n",

      "return -1;\n",

      "return +127;\n",

      "return -128;\n",

      "return 2.0;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("PrimitiveReturnStatements.golden")));
}

TEST(PrimitiveExpressions) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var x = 0; return x;\n",

      "var x = 0; return x + 3;\n",

      "var x = 0; return x - 3;\n",

      "var x = 4; return x * 3;\n",

      "var x = 4; return x / 3;\n",

      "var x = 4; return x % 3;\n",

      "var x = 1; return x | 2;\n",

      "var x = 1; return x ^ 2;\n",

      "var x = 1; return x & 2;\n",

      "var x = 10; return x << 3;\n",

      "var x = 10; return x >> 3;\n",

      "var x = 10; return x >>> 3;\n",

      "var x = 0; return (x, 3);\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("PrimitiveExpressions.golden")));
}

TEST(LogicalExpressions) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var x = 0; return x || 3;\n",

      "var x = 0; return (x == 1) || 3;\n",

      "var x = 0; return x && 3;\n",

      "var x = 0; return (x == 0) && 3;\n",

      "var x = 0; return x || (1, 2, 3);\n",

      "var a = 2, b = 3, c = 4; return a || (a, b, a, b, c = 5, 3);\n",

      "var x = 1; var a = 2, b = 3; return x || ("  //
      REPEAT_32("\n  a = 1, b = 2, ")               //
      "3);\n",

      "var x = 0; var a = 2, b = 3; return x && ("  //
      REPEAT_32("\n  a = 1, b = 2, ")               //
      "3);\n",

      "var x = 1; var a = 2, b = 3; return (x > 3) || ("  //
      REPEAT_32("\n  a = 1, b = 2, ")                     //
      "3);\n",

      "var x = 0; var a = 2, b = 3; return (x < 5) && ("  //
      REPEAT_32("\n  a = 1, b = 2, ")                     //
      "3);\n",

      "return 0 && 3;\n",

      "return 1 || 3;\n",

      "var x = 1; return x && 3 || 0, 1;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("LogicalExpressions.golden")));
}

TEST(Parameters) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function f() { return this; }",

      "function f(arg1) { return arg1; }",

      "function f(arg1) { return this; }",

      "function f(arg1, arg2, arg3, arg4, arg5, arg6, arg7) { return arg4; }",

      "function f(arg1, arg2, arg3, arg4, arg5, arg6, arg7) { return this; }",

      "function f(arg1) { arg1 = 1; }",

      "function f(arg1, arg2, arg3, arg4) { arg2 = 1; }",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets, "", "\nf();"),
                     LoadGolden("Parameters.golden")));
}

TEST(IntegerConstants) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "return 12345678;\n",

      "var a = 1234; return 5678;\n",

      "var a = 1234; return 1234;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("IntegerConstants.golden")));
}

TEST(HeapNumberConstants) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "return 1.2;\n",

      "var a = 1.2; return 2.6;\n",

      "var a = 3.14; return 3.14;\n",

      "var a;"                    //
      REPEAT_256("\na = 1.414;")  //
      " a = 3.14;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("HeapNumberConstants.golden")));
}

TEST(StringConstants) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "return \"This is a string\";\n",

      "var a = \"First string\"; return \"Second string\";\n",

      "var a = \"Same string\"; return \"Same string\";\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("StringConstants.golden")));
}

TEST(PropertyLoads) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
    "function f(a) { return a.name; }\n"
    "f({name : \"test\"});\n",

    "function f(a) { return a[\"key\"]; }\n"
    "f({key : \"test\"});\n",

    "function f(a) { return a[100]; }\n"
    "f({100 : \"test\"});\n",

    "function f(a, b) { return a[b]; }\n"
    "f({arg : \"test\"}, \"arg\");\n",

    "function f(a) { var b = a.name; return a[-124]; }\n"
    "f({\"-124\" : \"test\", name : 123 })",

    "function f(a) {\n"
    "  var b;\n"
    "  b = a.name;\n"
    REPEAT_127("  b = a.name;\n")
    "  return a.name;\n"
    "}\n"
    "f({name : \"test\"})\n",

    "function f(a, b) {\n"
    "  var c;\n"
    "  c = a[b];\n"
    REPEAT_127("  c = a[b];\n")
    "  return a[b];\n"
    "}\n"
    "f({name : \"test\"}, \"name\")\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("PropertyLoads.golden")));
}

TEST(PropertyStores) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
    "function f(a) { a.name = \"val\"; }\n"
    "f({name : \"test\"})",

    "function f(a) { a[\"key\"] = \"val\"; }\n"
    "f({key : \"test\"})",

    "function f(a) { a[100] = \"val\"; }\n"
    "f({100 : \"test\"})",

    "function f(a, b) { a[b] = \"val\"; }\n"
    "f({arg : \"test\"}, \"arg\")",

    "function f(a) { a.name = a[-124]; }\n"
    "f({\"-124\" : \"test\", name : 123 })",

    "function f(a) { \"use strict\"; a.name = \"val\"; }\n"
    "f({name : \"test\"})",

    "function f(a, b) { \"use strict\"; a[b] = \"val\"; }\n"
    "f({arg : \"test\"}, \"arg\")",

    "function f(a) {\n"
    "  a.name = 1;\n"
    REPEAT_127("  a.name = 1;\n")
    "  a.name = 2;\n"
    "}\n"
    "f({name : \"test\"})\n",

    "function f(a) {\n"
    " 'use strict';\n"
    "  a.name = 1;\n"
    REPEAT_127("  a.name = 1;\n")
    "  a.name = 2;\n"
    "}\n"
    "f({name : \"test\"})\n",

    "function f(a, b) {\n"
    "  a[b] = 1;\n"
    REPEAT_127("  a[b] = 1;\n")
    "  a[b] = 2;\n"
    "}\n"
    "f({name : \"test\"})\n",

    "function f(a, b) {\n"
    "  'use strict';\n"
    "  a[b] = 1;\n"
    REPEAT_127("  a[b] = 1;\n")
    "  a[b] = 2;\n"
    "}\n"
    "f({name : \"test\"})\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("PropertyStores.golden")));
}

#define FUNC_ARG "new (function Obj() { this.func = function() { return; }})()"

TEST(PropertyCall) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function f(a) { return a.func(); }\n"
      "f(" FUNC_ARG ")",

      "function f(a, b, c) { return a.func(b, c); }\n"
      "f(" FUNC_ARG ", 1, 2)",

      "function f(a, b) { return a.func(b + b, b); }\n"
      "f(" FUNC_ARG ", 1)",

      "function f(a) {\n"
      " a.func;\n"              //
      REPEAT_127(" a.func;\n")  //
      " return a.func(); }\n"
      "f(" FUNC_ARG ")",

      "function f(a) { return a.func(1).func(2).func(3); }\n"
      "f(new (function Obj() { this.func = function(a) { return this; }})())",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("PropertyCall.golden")));
}

TEST(LoadGlobal) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
    "var a = 1;\n"
    "function f() { return a; }\n"
    "f()",

    "function t() { }\n"
    "function f() { return t; }\n"
    "f()",

    "a = 1;\n"
    "function f() { return a; }\n"
    "f()",

    "a = 1;\n"
    "function f(b) {\n"
    "  b.name;\n"
    REPEAT_127("  b.name;\n")
    "  return a;\n"
    "}\n"
    "f({name: 1});\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("LoadGlobal.golden")));
}

TEST(StoreGlobal) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
    "var a = 1;\n"
    "function f() { a = 2; }\n"
    "f();\n",

    "var a = \"test\"; function f(b) { a = b; }\n"
    "f(\"global\");\n",

    "'use strict'; var a = 1;\n"
    "function f() { a = 2; }\n"
    "f();\n",

    "a = 1;\n"
    "function f() { a = 2; }\n"
    "f();\n",

    "a = 1;\n"
    "function f(b) {\n"
    "  b.name;\n"
    REPEAT_127("  b.name;\n")
    "  a = 2;\n"
    "}\n"
    "f({name: 1});\n",

    "a = 1;\n"
    "function f(b) {\n"
    "  'use strict';\n"
    "  b.name;\n"
    REPEAT_127("  b.name;\n")
    "  a = 2;\n"
    "}\n"
    "f({name: 1});\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("StoreGlobal.golden")));
}

TEST(CallGlobal) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function t() { }\n"
      "function f() { return t(); }\n"
      "f();\n",

      "function t(a, b, c) { }\n"
      "function f() { return t(1, 2, 3); }\n"
      "f();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("CallGlobal.golden")));
}

TEST(CallRuntime) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function f() { %TheHole() }\n"
      "f();\n",

      "function f(a) { return %IsArray(a) }\n"
      "f(undefined);\n",

      "function f() { return %Add(1, 2) }\n"
      "f();\n",

      "function f() { return %spread_iterable([1]) }\n"
      "f();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("CallRuntime.golden")));
}

TEST(IfConditions) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
    "function f() {\n"
    "  if (0) {\n"
    "    return 1;\n"
    "  } else {\n"
    "    return -1;\n"
    "  }\n"
    "};\n"
    "f();\n",

    "function f() {\n"
    "  if ('lucky') {\n"
    "    return 1;\n"
    "  } else {\n"
    "    return -1;\n"
    "  }\n"
    "};\n"
    "f();\n",

    "function f() {\n"
    "  if (false) {\n"
    "    return 1;\n"
    "  } else {\n"
    "    return -1;\n"
    "  }\n"
    "};\n"
    "f();\n",

    "function f() {\n"
    "  if (false) {\n"
    "    return 1;\n"
    "  }\n"
    "};\n"
    "f();\n",

    "function f() {\n"
    "  var a = 1;\n"
    "  if (a) {\n"
    "    a += 1;\n"
    "  } else {\n"
    "    return 2;\n"
    "  }\n"
    "};\n"
    "f();\n",

    "function f(a) {\n"
    "  if (a <= 0) {\n"
    "    return 200;\n"
    "  } else {\n"
    "    return -200;\n"
    "  }\n"
    "};\n"
    "f(99);\n",

    "function f(a, b) { if (a in b) { return 200; } }"
    "f('prop', { prop: 'yes'});\n",

    "function f(z) { var a = 0; var b = 0; if (a === 0.01) {\n"
    REPEAT_64("  b = a; a = b;\n")
    " return 200; } else { return -200; } } f(0.001);\n",

    "function f() {\n"
    "  var a = 0; var b = 0;\n"
    "  if (a) {\n"
    REPEAT_64("  b = a; a = b;\n")
    "  return 200; } else { return -200; }\n"
    "};\n"
    "f();\n",

    "function f(a, b) {\n"
    "  if (a == b) { return 1; }\n"
    "  if (a === b) { return 1; }\n"
    "  if (a < b) { return 1; }\n"
    "  if (a > b) { return 1; }\n"
    "  if (a <= b) { return 1; }\n"
    "  if (a >= b) { return 1; }\n"
    "  if (a in b) { return 1; }\n"
    "  if (a instanceof b) { return 1; }\n"
    "  return 0;\n"
    "}\n"
    "f(1, 1);\n",

    "function f() {\n"
    "  var a = 0;\n"
    "  if (a) {\n"
    "    return 20;\n"
    "  } else {\n"
    "    return -20;\n"
    "  }\n"
    "};\n"
    "f();\n",

    "function f(a, b) {\n"
    "  if (a == b || a < 0) {\n"
    "    return 1;\n"
    "  } else if (a > 0 && b > 0) {\n"
    "    return 0;\n"
    "  } else {\n"
    "    return -1;\n"
    "  }\n"
    "};\n"
    "f(-1, 1);\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("IfConditions.golden")));
}

TEST(DeclareGlobals) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");
  printer.set_top_level(true);

  const char* snippets[] = {
      "var a = 1;\n",

      "function f() {}\n",

      "var a = 1;\n"
      "a=2;\n",

      "function f() {}\n"
      "f();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("DeclareGlobals.golden")));
}

TEST(BreakableBlocks) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());

  const char* snippets[] = {
      "var x = 0;\n"
      "label: {\n"
      "  x = x + 1;\n"
      "  break label;\n"
      "  x = x + 1;\n"
      "}\n"
      "return x;\n",

      "var sum = 0;\n"
      "outer: {\n"
      "  for (var x = 0; x < 10; ++x) {\n"
      "    for (var y = 0; y < 3; ++y) {\n"
      "      ++sum;\n"
      "      if (x + y == 12) { break outer; }\n"
      "    }\n"
      "  }\n"
      "}\n"
      "return sum;\n",

      "outer: {\n"
      "  let y = 10;\n"
      "  function f() { return y; }\n"
      "  break outer;\n"
      "}\n",

      "let x = 1;\n"
      "outer: {\n"
      "  inner: {\n"
      "   let y = 2;\n"
      "    function f() { return x + y; }\n"
      "    if (y) break outer;\n"
      "    y = 3;\n"
      "  }\n"
      "}\n"
      "x = 4;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("BreakableBlocks.golden")));
}

TEST(BasicLoops) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var x = 0;\n"
      "while (false) { x = 99; break; continue; }\n"
      "return x;\n",

      "var x = 0;\n"
      "while (false) {\n"
      "  x = x + 1;\n"
      "};\n"
      "return x;\n",

      "var x = 0;\n"
      "var y = 1;\n"
      "while (x < 10) {\n"
      "  y = y * 12;\n"
      "  x = x + 1;\n"
      "  if (x == 3) continue;\n"
      "  if (x == 4) break;\n"
      "}\n"
      "return y;\n",

      "var i = 0;\n"
      "while (true) {\n"
      "  if (i < 0) continue;\n"
      "  if (i == 3) break;\n"
      "  if (i == 4) break;\n"
      "  if (i == 10) continue;\n"
      "  if (i == 5) break;\n"
      "  i = i + 1;\n"
      "}\n"
      "return i;\n",

      "var i = 0;\n"
      "while (true) {\n"
      "  while (i < 3) {\n"
      "    if (i == 2) break;\n"
      "    i = i + 1;\n"
      "  }\n"
      "  i = i + 1;\n"
      "  break;\n"
      "}\n"
      "return i;\n",

      "var x = 10;\n"
      "var y = 1;\n"
      "while (x) {\n"
      "  y = y * 12;\n"
      "  x = x - 1;\n"
      "}\n"
      "return y;\n",

      "var x = 0; var y = 1;\n"
      "do {\n"
      "  y = y * 10;\n"
      "  if (x == 5) break;\n"
      "  if (x == 6) continue;\n"
      "  x = x + 1;\n"
      "} while (x < 10);\n"
      "return y;\n",

      "var x = 10;\n"
      "var y = 1;\n"
      "do {\n"
      "  y = y * 12;\n"
      "  x = x - 1;\n"
      "} while (x);\n"
      "return y;\n",

      "var x = 0; var y = 1;\n"
      "do {\n"
      "  y = y * 10;\n"
      "  if (x == 5) break;\n"
      "  x = x + 1;\n"
      "  if (x == 6) continue;\n"
      "} while (false);\n"
      "return y;\n",

      "var x = 0; var y = 1;\n"
      "do {\n"
      "  y = y * 10;\n"
      "  if (x == 5) break;\n"
      "  x = x + 1;\n"
      "  if (x == 6) continue;\n"
      "} while (true);\n"
      "return y;\n",

      "var x = 0;\n"
      "for (;;) {\n"
      "  if (x == 1) break;\n"
      "  if (x == 2) continue;\n"
      "  x = x + 1;\n"
      "}\n",

      "for (var x = 0;;) {\n"
      "  if (x == 1) break;\n"
      "  if (x == 2) continue;\n"
      "  x = x + 1;\n"
      "}\n",

      "var x = 0;\n"
      "for (;; x = x + 1) {\n"
      "  if (x == 1) break;\n"
      "  if (x == 2) continue;\n"
      "}\n",

      "for (var x = 0;; x = x + 1) {\n"
      "  if (x == 1) break;\n"
      "  if (x == 2) continue;\n"
      "}\n",

      "var u = 0;\n"
      "for (var i = 0; i < 100; i = i + 1) {\n"
      "  u = u + 1;\n"
      "  continue;\n"
      "}\n",

      "var y = 1;\n"
      "for (var x = 10; x; --x) {\n"
      "  y = y * 12;\n"
      "}\n"
      "return y;\n",

      "var x = 0;\n"
      "for (var i = 0; false; i++) {\n"
      "  x = x + 1;\n"
      "};\n"
      "return x;\n",

      "var x = 0;\n"
      "for (var i = 0; true; ++i) {\n"
      "  x = x + 1;\n"
      "  if (x == 20) break;\n"
      "};\n"
      "return x;\n",

      "var a = 0;\n"
      "while (a) {\n"
      "  { \n"
      "   let z = 1;\n"
      "   function f() { z = 2; }\n"
      "   if (z) continue;\n"
      "   z++;\n"
      "  }\n"
      "}\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("BasicLoops.golden")));
}

TEST(JumpsRequiringConstantWideOperands) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
    REPEAT_256("var x = 0.1;\n")
    REPEAT_32("var x = 0.2;\n")
    REPEAT_16("var x = 0.3;\n")
    REPEAT_8("var x = 0.4;\n")
    "for (var i = 0; i < 3; i++) {\n"
    "  if (i == 1) continue;\n"
    "  if (i == 2) break;\n"
    "}\n"
    "return 3;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("JumpsRequiringConstantWideOperands.golden")));
}

TEST(UnaryOperators) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var x = 0;\n"
      "while (x != 10) {\n"
      "  x = x + 10;\n"
      "}\n"
      "return x;\n",

      "var x = false;\n"
      "do {\n"
      "  x = !x;\n"
      "} while(x == false);\n"
      "return x;\n",

      "var x = 101;\n"
      "return void(x * 3);\n",

      "var x = 1234;\n"
      "var y = void (x * x - 1);\n"
      "return y;\n",

      "var x = 13;\n"
      "return ~x;\n",

      "var x = 13;\n"
      "return +x;\n",

      "var x = 13;\n"
      "return -x;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("UnaryOperators.golden")));
}

TEST(Typeof) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function f() {\n"
      " var x = 13;\n"
      " return typeof(x);\n"
      "};",

      "var x = 13;\n"
      "function f() {\n"
      " return typeof(x);\n"
      "};",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets, "", "\nf();"),
                     LoadGolden("Typeof.golden")));
}

TEST(Delete) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());

  const char* snippets[] = {
      "var a = {x:13, y:14}; return delete a.x;\n",

      "'use strict'; var a = {x:13, y:14}; return delete a.x;\n",

      "var a = {1:13, 2:14}; return delete a[2];\n",

      "var a = 10; return delete a;\n",

      "'use strict';\n"
      "var a = {1:10};\n"
      "(function f1() {return a;});\n"
      "return delete a[1];\n",

      "return delete 'test';\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("Delete.golden")));
}

TEST(GlobalDelete) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "var a = {x:13, y:14};\n"
      "function f() {\n"
      "  return delete a.x;\n"
      "};\n"
      "f();\n",

      "a = {1:13, 2:14};\n"
      "function f() {\n"
      "  'use strict';\n"
      "  return delete a[1];\n"
      "};\n"
      "f();\n",

      "var a = {x:13, y:14};\n"
      "function f() {\n"
      "  return delete a;\n"
      "};\n"
      "f();\n",

      "b = 30;\n"
      "function f() {\n"
      "  return delete b;\n"
      "};\n"
      "f();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("GlobalDelete.golden")));
}

TEST(FunctionLiterals) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());

  const char* snippets[] = {
      "return function(){ }\n",

      "return (function(){ })()\n",

      "return (function(x){ return x; })(1)\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("FunctionLiterals.golden")));
}

TEST(RegExpLiterals) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());

  const char* snippets[] = {
      "return /ab+d/;\n",

      "return /(\\w+)\\s(\\w+)/i;\n",

      "return /ab+d/.exec('abdd');\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("RegExpLiterals.golden")));
}

TEST(RegExpLiteralsWide) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());

  const char* snippets[] = {
      "var a;"                   //
      REPEAT_256("\na = 1.23;")  //
      "\nreturn /ab+d/;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("RegExpLiteralsWide.golden")));
}

TEST(ArrayLiterals) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());

  const char* snippets[] = {
      "return [ 1, 2 ];\n",

      "var a = 1; return [ a, a + 1 ];\n",

      "return [ [ 1, 2 ], [ 3 ] ];\n",

      "var a = 1; return [ [ a, 2 ], [ a + 2 ] ];\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("ArrayLiterals.golden")));
}

TEST(ArrayLiteralsWide) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());

  const char* snippets[] = {
      "var a;"                   //
      REPEAT_256("\na = 1.23;")  //
      "\nreturn [ 1 , 2 ];\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("ArrayLiteralsWide.golden")));
}

TEST(ObjectLiterals) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());

  const char* snippets[] = {
      "return { };\n",

      "return { name: 'string', val: 9.2 };\n",

      "var a = 1; return { name: 'string', val: a };\n",

      "var a = 1; return { val: a, val: a + 1 };\n",

      "return { func: function() { } };\n",

      "return { func(a) { return a; } };\n",

      "return { get a() { return 2; } };\n",

      "return { get a() { return this.x; }, set a(val) { this.x = val } };\n",

      "return { set b(val) { this.y = val } };\n",

      "var a = 1; return { 1: a };\n",

      "return { __proto__: null };\n",

      "var a = 'test'; return { [a]: 1 };\n",

      "var a = 'test'; return { val: a, [a]: 1 };\n",

      "var a = 'test'; return { [a]: 1, __proto__: {} };\n",

      "var n = 'name'; return { [n]: 'val', get a() { }, set a(b) {} };\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("ObjectLiterals.golden")));
}

TEST(ObjectLiteralsWide) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var a;"                   //
      REPEAT_256("\na = 1.23;")  //
      "\nreturn { name: 'string', val: 9.2 };\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("ObjectLiteralsWide.golden")));
}

TEST(TopLevelObjectLiterals) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");
  printer.set_top_level(true);

  const char* snippets[] = {
      "var a = { func: function() { } };\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("TopLevelObjectLiterals.golden")));
}

TEST(TryCatch) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());

  const char* snippets[] = {
      "try { return 1; } catch(e) { return 2; }\n",

      "var a;\n"
      "try { a = 1 } catch(e1) {};\n"
      "try { a = 2 } catch(e2) { a = 3 }\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("TryCatch.golden")));
}

TEST(TryFinally) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var a = 1;\n"
      "try { a = 2; } finally { a = 3; }\n",

      "var a = 1;\n"
      "try { a = 2; } catch(e) { a = 20 } finally { a = 3; }\n",

      "var a; try {\n"
      "  try { a = 1 } catch(e) { a = 2 }\n"
      "} catch(e) { a = 20 } finally { a = 3; }\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("TryFinally.golden")));
}

TEST(Throw) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "throw 1;\n",

      "throw 'Error';\n",

      "var a = 1; if (a) { throw 'Error'; };\n",
  };

  CHECK(
      CompareTexts(BuildActual(printer, snippets), LoadGolden("Throw.golden")));
}

TEST(CallNew) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function bar() { this.value = 0; }\n"
      "function f() { return new bar(); }\n"
      "f();\n",

      "function bar(x) { this.value = 18; this.x = x;}\n"
      "function f() { return new bar(3); }\n"
      "f();\n",

      "function bar(w, x, y, z) {\n"
      "  this.value = 18;\n"
      "  this.x = x;\n"
      "  this.y = y;\n"
      "  this.z = z;\n"
      "}\n"
      "function f() { return new bar(3, 4, 5); }\n"
      "f();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("CallNew.golden")));
}

TEST(ContextVariables) {
  // The wide check below relies on MIN_CONTEXT_SLOTS + 3 + 249 == 256, if this
  // ever changes, the REPEAT_XXX should be changed to output the correct number
  // of unique variables to trigger the wide slot load / store.
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS + 3 + 249 == 256);

  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
    "var a; return function() { a = 1; };\n",

    "var a = 1; return function() { a = 2; };\n",

    "var a = 1; var b = 2; return function() { a = 2; b = 3 };\n",

    "var a; (function() { a = 2; })(); return a;\n",

    "'use strict';\n"
    "let a = 1;\n"
    "{ let b = 2; return function() { a + b; }; }\n",

    "'use strict';\n"
    REPEAT_250_UNIQUE_VARS()
    "eval();\n"
    "var b = 100;\n"
    "return b\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("ContextVariables.golden")));
}

TEST(ContextParameters) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function f(arg1) { return function() { arg1 = 2; }; }",

      "function f(arg1) { var a = function() { arg1 = 2; }; return arg1; }",

      "function f(a1, a2, a3, a4) { return function() { a1 = a3; }; }",

      "function f() { var self = this; return function() { self = 2; }; }",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets, "", "\nf();"),
                     LoadGolden("ContextParameters.golden")));
}

TEST(OuterContextVariables) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function Outer() {\n"
      "  var outerVar = 1;\n"
      "  function Inner(innerArg) {\n"
      "    this.innerFunc = function() { return outerVar * innerArg; }\n"
      "  }\n"
      "  this.getInnerFunc = function() { return new Inner(1).innerFunc; }\n"
      "}\n"
      "var f = new Outer().getInnerFunc();",

      "function Outer() {\n"
      "  var outerVar = 1;\n"
      "  function Inner(innerArg) {\n"
      "    this.innerFunc = function() { outerVar = innerArg; }\n"
      "  }\n"
      "  this.getInnerFunc = function() { return new Inner(1).innerFunc; }\n"
      "}\n"
      "var f = new Outer().getInnerFunc();",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets, "", "\nf();"),
                     LoadGolden("OuterContextVariables.golden")));
}

TEST(CountOperators) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var a = 1; return ++a;\n",

      "var a = 1; return a++;\n",

      "var a = 1; return --a;\n",

      "var a = 1; return a--;\n",

      "var a = { val: 1 }; return a.val++;\n",

      "var a = { val: 1 }; return --a.val;\n",

      "var name = 'var'; var a = { val: 1 }; return a[name]--;\n",

      "var name = 'var'; var a = { val: 1 }; return ++a[name];\n",

      "var a = 1; var b = function() { return a }; return ++a;\n",

      "var a = 1; var b = function() { return a }; return a--;\n",

      "var idx = 1; var a = [1, 2]; return a[idx++] = 2;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("CountOperators.golden")));
}

TEST(GlobalCountOperators) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "var global = 1;\n"
      "function f() { return ++global; }\n"
      "f();\n",

      "var global = 1;\n"
      "function f() { return global--; }\n"
      "f();\n",

      "unallocated = 1;\n"
      "function f() { 'use strict'; return --unallocated; }\n"
      "f();\n",

      "unallocated = 1;\n"
      "function f() { return unallocated++; }\n"
      "f();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("GlobalCountOperators.golden")));
}

TEST(CompoundExpressions) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var a = 1; a += 2;\n",

      "var a = 1; a /= 2;\n",

      "var a = { val: 2 }; a.name *= 2;\n",

      "var a = { 1: 2 }; a[1] ^= 2;\n",

      "var a = 1; (function f() { return a; }); a |= 24;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("CompoundExpressions.golden")));
}

TEST(GlobalCompoundExpressions) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "var global = 1;\n"
      "function f() { return global &= 1; }\n"
      "f();\n",

      "unallocated = 1;\n"
      "function f() { return unallocated += 1; }\n"
      "f();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("GlobalCompoundExpressions.golden")));
}

TEST(CreateArguments) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function f() { return arguments; }",

      "function f() { return arguments[0]; }",

      "function f() { 'use strict'; return arguments; }",

      "function f(a) { return arguments[0]; }",

      "function f(a, b, c) { return arguments; }",

      "function f(a, b, c) { 'use strict'; return arguments; }",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets, "", "\nf();"),
                     LoadGolden("CreateArguments.golden")));
}

TEST(CreateRestParameter) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function f(...restArgs) { return restArgs; }",

      "function f(a, ...restArgs) { return restArgs; }",

      "function f(a, ...restArgs) { return restArgs[0]; }",

      "function f(a, ...restArgs) { return restArgs[0] + arguments[0]; }",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets, "", "\nf();"),
                     LoadGolden("CreateRestParameter.golden")));
}

TEST(ForIn) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "for (var p in null) {}\n",

      "for (var p in undefined) {}\n",

      "for (var p in undefined) {}\n",

      "var x = 'potatoes';\n"
      "for (var p in x) { return p; }\n",

      "var x = 0;\n"
      "for (var p in [1,2,3]) { x += p; }\n",

      "var x = { 'a': 1, 'b': 2 };\n"
      "for (x['a'] in [10, 20, 30]) {\n"
      "  if (x['a'] == 10) continue;\n"
      "  if (x['a'] == 20) break;\n"
      "}\n",

      "var x = [ 10, 11, 12 ] ;\n"
      "for (x[0] in [1,2,3]) { return x[3]; }\n",
  };

  CHECK(
      CompareTexts(BuildActual(printer, snippets), LoadGolden("ForIn.golden")));
}

TEST(ForOf) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "for (var p of [0, 1, 2]) {}\n",

      "var x = 'potatoes';\n"
      "for (var p of x) { return p; }\n",

      "for (var x of [10, 20, 30]) {\n"
      "  if (x == 10) continue;\n"
      "  if (x == 20) break;\n"
      "}\n",

      "var x = { 'a': 1, 'b': 2 };\n"
      "for (x['a'] of [1,2,3]) { return x['a']; }\n",
  };

  CHECK(
      CompareTexts(BuildActual(printer, snippets), LoadGolden("ForOf.golden")));
}

TEST(Conditional) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "return 1 ? 2 : 3;\n",

      "return 1 ? 2 ? 3 : 4 : 5;\n",

      "return 0 < 1 ? 2 : 3;\n",

      "var x = 0;\n"
      "return x ? 2 : 3;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("Conditional.golden")));
}

TEST(Switch) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
    "var a = 1;\n"
    "switch(a) {\n"
    " case 1: return 2;\n"
    " case 2: return 3;\n"
    "}\n",

    "var a = 1;\n"
    "switch(a) {\n"
    " case 1: a = 2; break;\n"
    " case 2: a = 3; break;\n"
    "}\n",

    "var a = 1;\n"
    "switch(a) {\n"
    " case 1: a = 2; // fall-through\n"
    " case 2: a = 3; break;\n"
    "}\n",

    "var a = 1;\n"
    "switch(a) {\n"
    " case 2: break;\n"
    " case 3: break;\n"
    " default: a = 1; break;\n"
    "}\n",

    "var a = 1;\n"
    "switch(typeof(a)) {\n"
    " case 2: a = 1; break;\n"
    " case 3: a = 2; break;\n"
    " default: a = 3; break;\n"
    "}\n",

    "var a = 1;\n"
    "switch(a) {\n"
    " case typeof(a): a = 1; break;\n"
    " default: a = 2; break;\n"
    "}\n",

    "var a = 1;\n"
    "switch(a) {\n"
    " case 1:\n"
    REPEAT_64("  a = 2;\n")
    "  break;\n"
    " case 2:\n"
    "  a = 3;\n"
    "  break;\n"
    "}\n",

    "var a = 1;\n"
    "switch(a) {\n"
    " case 1: \n"
    "   switch(a + 1) {\n"
    "      case 2 : a = 1; break;\n"
    "      default : a = 2; break;\n"
    "   }  // fall-through\n"
    " case 2: a = 3;\n"
    "}\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("Switch.golden")));
}

TEST(BasicBlockToBoolean) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var a = 1; if (a || a < 0) { return 1; }\n",

      "var a = 1; if (a && a < 0) { return 1; }\n",

      "var a = 1; a = (a || a < 0) ? 2 : 3;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("BasicBlockToBoolean.golden")));
}

TEST(DeadCodeRemoval) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "return; var a = 1; a();\n",

      "if (false) { return; }; var a = 1;\n",

      "if (true) { return 1; } else { return 2; };\n",

      "var a = 1; if (a) { return 1; }; return 2;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("DeadCodeRemoval.golden")));
}

TEST(ThisFunction) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "var f;\n"
      "f = function f() {};",

      "var f;\n"
      "f = function f() { return f; };",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets, "", "\nf();"),
                     LoadGolden("ThisFunction.golden")));
}

TEST(NewTarget) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());

  const char* snippets[] = {
      "return new.target;\n",

      "new.target;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("NewTarget.golden")));
}

TEST(RemoveRedundantLdar) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var ld_a = 1;\n"          // This test is to check Ldar does not
      "while(true) {\n"          // get removed if the preceding Star is
      "  ld_a = ld_a + ld_a;\n"  // in a different basicblock.
      "  if (ld_a > 10) break;\n"
      "}\n"
      "return ld_a;\n",

      "var ld_a = 1;\n"
      "do {\n"
      "  ld_a = ld_a + ld_a;\n"
      "  if (ld_a > 10) continue;\n"
      "} while(false);\n"
      "return ld_a;\n",

      "var ld_a = 1;\n"
      "  ld_a = ld_a + ld_a;\n"
      "  return ld_a;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("RemoveRedundantLdar.golden")));
}

TEST(GenerateTestUndetectable) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var obj_a = {val:1};\n"
      "var b = 10;\n"
      "if (obj_a == null) { b = 20;}\n"
      "return b;\n",

      "var obj_a = {val:1};\n"
      "var b = 10;\n"
      "if (obj_a == undefined) { b = 20;}\n"
      "return b;\n",

      "var obj_a = {val:1};\n"
      "var b = 10;\n"
      "if (obj_a != null) { b = 20;}\n"
      "return b;\n",

      "var obj_a = {val:1};\n"
      "var b = 10;\n"
      "if (obj_a != undefined) { b = 20;}\n"
      "return b;\n",

      "var obj_a = {val:1};\n"
      "var b = 10;\n"
      "if (obj_a === null) { b = 20;}\n"
      "return b;\n",

      "var obj_a = {val:1};\n"
      "var b = 10;\n"
      "if (obj_a === undefined) { b = 20;}\n"
      "return b;\n",

      "var obj_a = {val:1};\n"
      "var b = 10;\n"
      "if (obj_a !== null) { b = 20;}\n"
      "return b;\n",

      "var obj_a = {val:1};\n"
      "var b = 10;\n"
      "if (obj_a !== undefined) { b = 20;}\n"
      "return b;\n"};

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("GenerateTestUndetectable.golden")));
}

TEST(AssignmentsInBinaryExpression) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var x = 0, y = 1;\n"
      "return (x = 2, y = 3, x = 4, y = 5);\n",

      "var x = 55;\n"
      "var y = (x = 100);\n"
      "return y;\n",

      "var x = 55;\n"
      "x = x + (x = 100) + (x = 101);\n"
      "return x;\n",

      "var x = 55;\n"
      "x = (x = 56) - x + (x = 57);\n"
      "x++;\n"
      "return x;\n",

      "var x = 55;\n"
      "var y = x + (x = 1) + (x = 2) + (x = 3);\n"
      "return y;\n",

      "var x = 55;\n"
      "var x = x + (x = 1) + (x = 2) + (x = 3);\n"
      "return x;\n",

      "var x = 10, y = 20;\n"
      "return x + (x = 1) + (x + 1) * (y = 2) + (y = 3) + (x = 4) + (y = 5) + "
      "y;\n",

      "var x = 17;\n"
      "return 1 + x + (x++) + (++x);\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("AssignmentsInBinaryExpression.golden")));
}

TEST(Eval) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "return eval('1;');\n",
  };

  CHECK(
      CompareTexts(BuildActual(printer, snippets), LoadGolden("Eval.golden")));
}

TEST(LookupSlot) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_test_function_name("f");

  // clang-format off
  const char* snippets[] = {
      "eval('var x = 10;'); return x;\n",

      "eval('var x = 10;'); return typeof x;\n",

      "x = 20; return eval('');\n",

      "var x = 20;\n"
      "f = function(){\n"
      "  eval('var x = 10');\n"
      "  return x;\n"
      "}\n"
      "f();\n",

      "x = 20;\n"
      "f = function(){\n"
      "  eval('var x = 10');\n"
      "  return x;\n"
      "}\n"
      "f();\n"
  };
  // clang-format on

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("LookupSlot.golden")));
}

TEST(CallLookupSlot) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "g = function(){}; eval(''); return g();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("CallLookupSlot.golden")));
}

// TODO(mythria): tests for variable/function declaration in lookup slots.

TEST(LookupSlotInEval) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "return x;",

      "x = 10;",

      "'use strict'; x = 10;",

      "return typeof x;",
  };

  std::string actual = BuildActual(printer, snippets,
                                   "var f;\n"
                                   "var x = 1;\n"
                                   "function f1() {\n"
                                   "  eval(\"function t() { ",

                                   " }; f = t; f();\");\n"
                                   "}\n"
                                   "f1();");

  CHECK(CompareTexts(actual, LoadGolden("LookupSlotInEval.golden")));
}

TEST(LookupSlotWideInEval) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      REPEAT_256("    \"var y = 2.3;\" +\n")  //
      "    \"return x;\" +\n",

      REPEAT_256("    \"var y = 2.3;\" +\n")  //
      "    \"return typeof x;\" +\n",

      REPEAT_256("    \"var y = 2.3;\" +\n")  //
      "    \"x = 10;\" +\n",

      "    \"'use strict';\" +\n"             //
      REPEAT_256("    \"var y = 2.3;\" +\n")  //
      "    \"x = 10;\" +\n",
  };

  std::string actual = BuildActual(printer, snippets,
                                   "var f;\n"
                                   "var x = 1;\n"
                                   "function f1() {\n"
                                   "  eval(\"function t() {\" +\n",

                                   "  \"};\" +\n"
                                   "  \"f = t; f();\"\n);\n"
                                   "}\n"
                                   "f1();");

  CHECK(CompareTexts(actual, LoadGolden("LookupSlotWideInEval.golden")));
}

TEST(DeleteLookupSlotInEval) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "delete x;",

      "return delete y;",

      "return delete z;",
  };

  std::string actual = BuildActual(printer, snippets,
                                   "var f;\n"
                                   "var x = 1;\n"
                                   "z = 10;\n"
                                   "function f1() {\n"
                                   "  var y;\n"
                                   "  eval(\"function t() { ",

                                   " }; f = t; f();\");\n"
                                   "}\n"
                                   "f1();");

  CHECK(CompareTexts(actual, LoadGolden("DeleteLookupSlotInEval.golden")));
}

TEST(WideRegisters) {
  // Prepare prologue that creates frame for lots of registers.
  std::ostringstream os;
  for (size_t i = 0; i < 157; ++i) {
    os << "var x" << i << ";\n";
  }
  std::string prologue(os.str());

  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "x0 = x127;\n"
      "return x0;\n",

      "x127 = x126;\n"
      "return x127;\n",

      "if (x2 > 3) { return x129; }\n"
      "return x128;\n",

      "var x0 = 0;\n"
      "if (x129 == 3) { var x129 = x0; }\n"
      "if (x2 > 3) { return x0; }\n"
      "return x129;\n",

      "var x0 = 0;\n"
      "var x1 = 0;\n"
      "for (x128 = 0; x128 < 64; x128++) {"
      "  x1 += x128;"
      "}"
      "return x128;\n",

      "var x0 = 1234;\n"
      "var x1 = 0;\n"
      "for (x128 in x0) {"
      "  x1 += x128;"
      "}"
      "return x1;\n",

      "x0 = %Add(x64, x63);\n"
      "x1 = %Add(x27, x143);\n"
      "%TheHole();\n"
      "return x1;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets, prologue.c_str()),
                     LoadGolden("WideRegisters.golden")));
}

TEST(ConstVariable) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "const x = 10;\n",

      "const x = 10; return x;\n",

      "const x = ( x = 20);\n",

      "const x = 10; x = 20;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("ConstVariable.golden")));
}

TEST(LetVariable) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "let x = 10;\n",

      "let x = 10; return x;\n",

      "let x = (x = 20);\n",

      "let x = 10; x = 20;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("LetVariable.golden")));
}

TEST(ConstVariableContextSlot) {
  // TODO(mythria): Add tests for initialization of this via super calls.
  // TODO(mythria): Add tests that walk the context chain.
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "const x = 10; function f1() {return x;}\n",

      "const x = 10; function f1() {return x;} return x;\n",

      "const x = (x = 20); function f1() {return x;}\n",

      "const x = 10; x = 20; function f1() {return x;}\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("ConstVariableContextSlot.golden")));
}

TEST(LetVariableContextSlot) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "let x = 10; function f1() {return x;}\n",

      "let x = 10; function f1() {return x;} return x;\n",

      "let x = (x = 20); function f1() {return x;}\n",

      "let x = 10; x = 20; function f1() {return x;}\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("LetVariableContextSlot.golden")));
}

TEST(DoExpression) {
  bool old_flag = FLAG_harmony_do_expressions;
  FLAG_harmony_do_expressions = true;

  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "var a = do { }; return a;\n",

      "var a = do { var x = 100; }; return a;\n",

      "while(true) { var a = 10; a = do { ++a; break; }; a = 20; }\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("DoExpression.golden")));

  FLAG_harmony_do_expressions = old_flag;
}

TEST(WithStatement) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "with ({x:42}) { return x; }\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("WithStatement.golden")));
}

TEST(DoDebugger) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "debugger;\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("DoDebugger.golden")));
}

TEST(ClassDeclarations) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  const char* snippets[] = {
      "class Person {\n"
      "  constructor(name) { this.name = name; }\n"
      "  speak() { console.log(this.name + ' is speaking.'); }\n"
      "}\n",

      "class person {\n"
      "  constructor(name) { this.name = name; }\n"
      "  speak() { console.log(this.name + ' is speaking.'); }\n"
      "}\n",

      "var n0 = 'a';\n"
      "var n1 = 'b';\n"
      "class N {\n"
      "  [n0]() { return n0; }\n"
      "  static [n1]() { return n1; }\n"
      "}\n",

      "var count = 0;\n"
      "class C { constructor() { count++; }}\n"
      "return new C();\n",

      "(class {})\n"
      "class E { static name () {}}\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("ClassDeclarations.golden")));
}

TEST(ClassAndSuperClass) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("test");
  const char* snippets[] = {
      "var test;\n"
      "(function() {\n"
      "  class A {\n"
      "    method() { return 2; }\n"
      "  }\n"
      "  class B extends A {\n"
      "    method() { return super.method() + 1; }\n"
      "  }\n"
      "  test = new B().method;\n"
      "  test();\n"
      "})();\n",

      "var test;\n"
      "(function() {\n"
      "  class A {\n"
      "    get x() { return 1; }\n"
      "    set x(val) { return; }\n"
      "  }\n"
      "  class B extends A {\n"
      "    method() { super.x = 2; return super.x; }\n"
      "  }\n"
      "  test = new B().method;\n"
      "  test();\n"
      "})();\n",

      "var test;\n"
      "(function() {\n"
      "  class A {\n"
      "    constructor(x) { this.x_ = x; }\n"
      "  }\n"
      "  class B extends A {\n"
      "    constructor() { super(1); this.y_ = 2; }\n"
      "  }\n"
      "  test = new B().constructor;\n"
      "})();\n",

      "var test;\n"
      "(function() {\n"
      "  class A {\n"
      "    constructor() { this.x_ = 1; }\n"
      "  }\n"
      "  class B extends A {\n"
      "    constructor() { super(); this.y_ = 2; }\n"
      "  }\n"
      "  test = new B().constructor;\n"
      "})();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("ClassAndSuperClass.golden")));
}

TEST(Generators) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("f");

  const char* snippets[] = {
      "function* f() { }\n"
      "f();\n",

      "function* f() { yield 42 }\n"
      "f();\n",

      "function* f() { for (let x of [42]) yield x }\n"
      "f();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("Generators.golden")));
}

TEST(Modules) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_module(true);
  printer.set_top_level(true);

  const char* snippets[] = {
      "import \"bar\";\n",

      "import {foo} from \"bar\";\n",

      "import {foo as goo} from \"bar\";\n"
      "goo(42);\n"
      "{ let x; { goo(42) } };\n",

      "export var foo = 42;\n"
      "foo++;\n"
      "{ let x; { foo++ } };\n",

      "export let foo = 42;\n"
      "foo++;\n"
      "{ let x; { foo++ } };\n",

      "export const foo = 42;\n"
      "foo++;\n"
      "{ let x; { foo++ } };\n",

      "export default (function () {});\n",

      "export default (class {});\n",

      "export {foo as goo} from \"bar\"\n",

      "export * from \"bar\"\n",

      "import * as foo from \"bar\"\n"
      "foo.f(foo, foo.x);\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("Modules.golden")));
}

TEST(SuperCallAndSpread) {
  InitializedIgnitionHandleScope scope;
  BytecodeExpectationsPrinter printer(CcTest::isolate());
  printer.set_wrap(false);
  printer.set_test_function_name("test");
  const char* snippets[] = {
      "var test;\n"
      "(function() {\n"
      "  class A {\n"
      "    constructor(...args) { this.baseArgs = args; }\n"
      "  }\n"
      "  class B extends A {}\n"
      "  test = new B(1, 2, 3).constructor;\n"
      "})();\n",

      "var test;\n"
      "(function() {\n"
      "  class A {\n"
      "    constructor(...args) { this.baseArgs = args; }\n"
      "  }\n"
      "  class B extends A {\n"
      "    constructor(...args) { super(1, ...args); }\n"
      "  }\n"
      "  test = new B(1, 2, 3).constructor;\n"
      "})();\n",

      "var test;\n"
      "(function() {\n"
      "  class A {\n"
      "    constructor(...args) { this.baseArgs = args; }\n"
      "  }\n"
      "  class B extends A {\n"
      "    constructor(...args) { super(1, ...args, 1); }\n"
      "  }\n"
      "  test = new B(1, 2, 3).constructor;\n"
      "})();\n",
  };

  CHECK(CompareTexts(BuildActual(printer, snippets),
                     LoadGolden("SuperCallAndSpread.golden")));
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
