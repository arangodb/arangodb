// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api-inl.h"
#include "src/ast/ast.h"
#include "src/compiler.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/parsing/preparsed-scope-data-impl.h"
#include "src/parsing/preparsed-scope-data.h"

#include "test/cctest/cctest.h"
#include "test/cctest/scope-test-helper.h"
#include "test/cctest/unicode-helpers.h"

namespace {

enum SkipTests {
  DONT_SKIP = 0,
  // Skip if the test function declares itself strict, otherwise don't skip.
  SKIP_STRICT_FUNCTION = 1,
  // Skip if there's a "use strict" directive above the test.
  SKIP_STRICT_OUTER = 1 << 1,
  SKIP_ARROW = 1 << 2,
  SKIP_STRICT = SKIP_STRICT_FUNCTION | SKIP_STRICT_OUTER
};

enum class PreciseMaybeAssigned { YES, NO };

enum class Bailout { BAILOUT_IF_OUTER_SLOPPY, NO };

}  // namespace

TEST(PreParserScopeAnalysis) {
  i::FLAG_lazy_inner_functions = true;
  i::FLAG_preparser_scope_analysis = true;
  i::FLAG_aggressive_lazy_inner_functions = true;
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  LocalContext env;

  struct {
    const char* code;
    bool strict_outer;
    bool strict_test_function;
    bool arrow;
  } outers[] = {
      // Normal case (test function at the laziness boundary):
      {"function test(%s) { %s function skippable() { } } test;", false, false,
       false},

      {"var test2 = function test(%s) { %s function skippable() { } }; test2",
       false, false, false},

      // Arrow functions (they can never be at the laziness boundary):
      {"function test() { (%s) => { %s }; function skippable() { } } test;",
       false, false, true},

      // Repeat the above mentioned cases with global 'use strict'
      {"'use strict'; function test(%s) { %s function skippable() { } } test;",
       true, false, false},

      {"'use strict'; var test2 = function test(%s) { %s \n"
       "function skippable() { } }; test2",
       true, false, false},

      {"'use strict'; function test() { (%s) => { %s };\n"
       "function skippable() { } } test;",
       true, false, true},

      // ... and with the test function declaring itself strict:
      {"function test(%s) { 'use strict'; %s function skippable() { } } test;",
       false, true, false},

      {"var test2 = function test(%s) { 'use strict'; %s \n"
       "function skippable() { } }; test2",
       false, true, false},

      {"function test() { 'use strict'; (%s) => { %s };\n"
       "function skippable() { } } test;",
       false, true, true},

      // Methods containing skippable functions.
      {"function get_method() {\n"
       "  class MyClass { test_method(%s) { %s function skippable() { } } }\n"
       "  var o = new MyClass(); return o.test_method;\n"
       "}\n"
       "get_method();",
       true, true, false},

      // Corner case: function expression with name "arguments".
      {"var test = function arguments(%s) { %s function skippable() { } };\n"
       "test;\n",
       false, false, false}

      // FIXME(marja): Generators and async functions
  };

  struct Inner {
    Inner(const char* s) : source(s) {}  // NOLINT
    Inner(const char* s, SkipTests skip) : source(s), skip(skip) {}
    Inner(const char* s, SkipTests skip, PreciseMaybeAssigned precise)
        : source(s), skip(skip), precise_maybe_assigned(precise) {}

    Inner(const char* p, const char* s) : params(p), source(s) {}
    Inner(const char* p, const char* s, SkipTests skip)
        : params(p), source(s), skip(skip) {}
    Inner(const char* p, const char* s, SkipTests skip,
          PreciseMaybeAssigned precise)
        : params(p), source(s), skip(skip), precise_maybe_assigned(precise) {}
    Inner(const char* p, const char* s, SkipTests skip, Bailout bailout)
        : params(p), source(s), skip(skip), bailout(bailout) {}

    Inner(const char* s, std::function<void()> p, std::function<void()> e)
        : source(s), prologue(p), epilogue(e) {}

    const char* params = "";
    const char* source;
    SkipTests skip = DONT_SKIP;
    PreciseMaybeAssigned precise_maybe_assigned = PreciseMaybeAssigned::YES;
    Bailout bailout = Bailout::NO;
    std::function<void()> prologue = nullptr;
    std::function<void()> epilogue = nullptr;
  } inners[] = {
      // Simple cases
      {"var1;"},
      {"var1 = 5;"},
      {"if (true) {}"},
      {"function f1() {}"},
      {"test;"},
      {"test2;"},

      // Var declarations and assignments.
      {"var var1;"},
      {"var var1; var1 = 5;"},
      {"if (true) { var var1; }", DONT_SKIP, PreciseMaybeAssigned::NO},
      {"if (true) { var var1; var1 = 5; }"},
      {"var var1; function f() { var1; }"},
      {"var var1; var1 = 5; function f() { var1; }"},
      {"var var1; function f() { var1 = 5; }"},
      {"function f1() { f2(); } function f2() {}"},

      // Let declarations and assignments.
      {"let var1;"},
      {"let var1; var1 = 5;"},
      {"if (true) { let var1; }"},
      {"if (true) { let var1; var1 = 5; }"},
      {"let var1; function f() { var1; }"},
      {"let var1; var1 = 5; function f() { var1; }"},
      {"let var1; function f() { var1 = 5; }"},

      // Const declarations.
      {"const var1 = 5;"},
      {"if (true) { const var1 = 5; }"},
      {"const var1 = 5; function f() { var1; }"},

      // Functions.
      {"function f1() { let var2; }"},
      {"var var1 = function f1() { let var2; };"},
      {"let var1 = function f1() { let var2; };"},
      {"const var1 = function f1() { let var2; };"},
      {"var var1 = function() { let var2; };"},
      {"let var1 = function() { let var2; };"},
      {"const var1 = function() { let var2; };"},

      {"function *f1() { let var2; }"},
      {"let var1 = function *f1() { let var2; };"},
      {"let var1 = function*() { let var2; };"},

      {"async function f1() { let var2; }"},
      {"let var1 = async function f1() { let var2; };"},
      {"let var1 = async function() { let var2; };"},

      // Redeclarations.
      {"var var1; var var1;"},
      {"var var1; var var1; var1 = 5;"},
      {"var var1; if (true) { var var1; }"},
      {"if (true) { var var1; var var1; }"},
      {"var var1; if (true) { var var1; var1 = 5; }"},
      {"if (true) { var var1; var var1; var1 = 5; }"},
      {"var var1; var var1; function f() { var1; }"},
      {"var var1; var var1; function f() { var1 = 5; }"},

      // Shadowing declarations.
      {"var var1; if (true) { var var1; }"},
      {"var var1; if (true) { let var1; }"},
      {"let var1; if (true) { let var1; }"},

      {"var var1; if (true) { const var1 = 0; }"},
      {"const var1 = 0; if (true) { const var1 = 0; }"},

      // Variables deeper in the subscopes (scopes without variables inbetween).
      {"if (true) { if (true) { function f() { var var1 = 5; } } }"},

      // Arguments and this.
      {"arguments;"},
      {"arguments = 5;", SKIP_STRICT},
      {"if (true) { arguments; }"},
      {"if (true) { arguments = 5; }", SKIP_STRICT},
      {"() => { arguments; };"},
      {"var1, var2, var3", "arguments;"},
      {"var1, var2, var3", "arguments = 5;", SKIP_STRICT},
      {"var1, var2, var3", "() => { arguments; };"},
      {"var1, var2, var3", "() => { arguments = 5; };", SKIP_STRICT},

      {"this;"},
      {"if (true) { this; }"},
      {"() => { this; };"},

      // Variable called "arguments"
      {"var arguments;", SKIP_STRICT},
      {"var arguments; arguments = 5;", SKIP_STRICT},
      {"if (true) { var arguments; }", SKIP_STRICT, PreciseMaybeAssigned::NO},
      {"if (true) { var arguments; arguments = 5; }", SKIP_STRICT},
      {"var arguments; function f() { arguments; }", SKIP_STRICT},
      {"var arguments; arguments = 5; function f() { arguments; }",
       SKIP_STRICT},
      {"var arguments; function f() { arguments = 5; }", SKIP_STRICT},

      {"let arguments;", SKIP_STRICT},
      {"let arguments; arguments = 5;", SKIP_STRICT},
      {"if (true) { let arguments; }", SKIP_STRICT},
      {"if (true) { let arguments; arguments = 5; }", SKIP_STRICT},
      {"let arguments; function f() { arguments; }", SKIP_STRICT},
      {"let arguments; arguments = 5; function f() { arguments; }",
       SKIP_STRICT},
      {"let arguments; function f() { arguments = 5; }", SKIP_STRICT},

      {"const arguments = 5;", SKIP_STRICT},
      {"if (true) { const arguments = 5; }", SKIP_STRICT},
      {"const arguments = 5; function f() { arguments; }", SKIP_STRICT},

      // Destructuring declarations.
      {"var [var1, var2] = [1, 2];"},
      {"var [var1, var2, [var3, var4]] = [1, 2, [3, 4]];"},
      {"var [{var1: var2}, {var3: var4}] = [{var1: 1}, {var3: 2}];"},
      {"var [var1, ...var2] = [1, 2, 3];"},

      {"var {var1: var2, var3: var4} = {var1: 1, var3: 2};"},
      {"var {var1: var2, var3: {var4: var5}} = {var1: 1, var3: {var4: 2}};"},
      {"var {var1: var2, var3: [var4, var5]} = {var1: 1, var3: [2, 3]};"},

      {"let [var1, var2] = [1, 2];"},
      {"let [var1, var2, [var3, var4]] = [1, 2, [3, 4]];"},
      {"let [{var1: var2}, {var3: var4}] = [{var1: 1}, {var3: 2}];"},
      {"let [var1, ...var2] = [1, 2, 3];"},

      {"let {var1: var2, var3: var4} = {var1: 1, var3: 2};"},
      {"let {var1: var2, var3: {var4: var5}} = {var1: 1, var3: {var4: 2}};"},
      {"let {var1: var2, var3: [var4, var5]} = {var1: 1, var3: [2, 3]};"},

      {"const [var1, var2] = [1, 2];"},
      {"const [var1, var2, [var3, var4]] = [1, 2, [3, 4]];"},
      {"const [{var1: var2}, {var3: var4}] = [{var1: 1}, {var3: 2}];"},
      {"const [var1, ...var2] = [1, 2, 3];"},

      {"const {var1: var2, var3: var4} = {var1: 1, var3: 2};"},
      {"const {var1: var2, var3: {var4: var5}} = {var1: 1, var3: {var4: 2}};"},
      {"const {var1: var2, var3: [var4, var5]} = {var1: 1, var3: [2, 3]};"},

      // Referencing the function variable.
      {"test;"},
      {"function f1() { f1; }"},
      {"function f1() { function f2() { f1; } }"},
      {"function arguments() {}", SKIP_STRICT},
      {"function f1() {} function f1() {}", SKIP_STRICT},
      {"var f1; function f1() {}"},

      // Assigning to the function variable.
      {"test = 3;"},
      {"function f1() { f1 = 3; }"},
      {"function f1() { f1; } f1 = 3;"},
      {"function arguments() {} arguments = 8;", SKIP_STRICT},
      {"function f1() {} f1 = 3; function f1() {}", SKIP_STRICT},

      // Evals.
      {"var var1; eval('');"},
      {"var var1; function f1() { eval(''); }"},
      {"let var1; eval('');"},
      {"let var1; function f1() { eval(''); }"},
      {"const var1 = 10; eval('');"},
      {"const var1 = 10; function f1() { eval(''); }"},

      // Standard for loops.
      {"for (var var1 = 0; var1 < 10; ++var1) { }"},
      {"for (let var1 = 0; var1 < 10; ++var1) { }"},
      {"for (const var1 = 0; var1 < 10; ++var1) { }"},

      {"for (var var1 = 0; var1 < 10; ++var1) { function foo() { var1; } }"},
      {"for (let var1 = 0; var1 < 10; ++var1) { function foo() { var1; } }"},
      {"for (const var1 = 0; var1 < 10; ++var1) { function foo() { var1; } }"},

      // For of loops
      {"for (var1 of [1, 2]) { }"},
      {"for (var var1 of [1, 2]) { }"},
      {"for (let var1 of [1, 2]) { }"},
      {"for (const var1 of [1, 2]) { }"},

      {"for (var1 of [1, 2]) { var1; }"},
      {"for (var var1 of [1, 2]) { var1; }"},
      {"for (let var1 of [1, 2]) { var1; }"},
      {"for (const var1 of [1, 2]) { var1; }"},

      {"for (var1 of [1, 2]) { var1 = 0; }"},
      {"for (var var1 of [1, 2]) { var1 = 0; }"},
      {"for (let var1 of [1, 2]) { var1 = 0; }"},
      {"for (const var1 of [1, 2]) { var1 = 0; }"},

      {"for (var1 of [1, 2]) { function foo() { var1; } }"},
      {"for (var var1 of [1, 2]) { function foo() { var1; } }"},
      {"for (let var1 of [1, 2]) { function foo() { var1; } }"},
      {"for (const var1 of [1, 2]) { function foo() { var1; } }"},

      {"for (var1 of [1, 2]) { function foo() { var1 = 0; } }"},
      {"for (var var1 of [1, 2]) { function foo() { var1 = 0; } }"},
      {"for (let var1 of [1, 2]) { function foo() { var1 = 0; } }"},
      {"for (const var1 of [1, 2]) { function foo() { var1 = 0; } }"},

      // For in loops
      {"for (var1 in {a: 6}) { }"},
      {"for (var var1 in {a: 6}) { }"},
      {"for (let var1 in {a: 6}) { }"},
      {"for (const var1 in {a: 6}) { }"},

      {"for (var1 in {a: 6}) { var1; }"},
      {"for (var var1 in {a: 6}) { var1; }"},
      {"for (let var1 in {a: 6}) { var1; }"},
      {"for (const var1 in {a: 6}) { var1; }"},

      {"for (var1 in {a: 6}) { var1 = 0; }"},
      {"for (var var1 in {a: 6}) { var1 = 0; }"},
      {"for (let var1 in {a: 6}) { var1 = 0; }"},
      {"for (const var1 in {a: 6}) { var1 = 0; }"},

      {"for (var1 in {a: 6}) { function foo() { var1; } }"},
      {"for (var var1 in {a: 6}) { function foo() { var1; } }"},
      {"for (let var1 in {a: 6}) { function foo() { var1; } }"},
      {"for (const var1 in {a: 6}) { function foo() { var1; } }"},

      {"for (var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (var var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (let var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (const var1 in {a: 6}) { function foo() { var1 = 0; } }"},

      {"for (var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (var var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (let var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (const var1 in {a: 6}) { function foo() { var1 = 0; } }"},

      // Destructuring loop variable
      {"for ([var1, var2] of [[1, 1], [2, 2]]) { }"},
      {"for (var [var1, var2] of [[1, 1], [2, 2]]) { }"},
      {"for (let [var1, var2] of [[1, 1], [2, 2]]) { }"},
      {"for (const [var1, var2] of [[1, 1], [2, 2]]) { }"},

      {"for ([var1, var2] of [[1, 1], [2, 2]]) { var2 = 3; }"},
      {"for (var [var1, var2] of [[1, 1], [2, 2]]) { var2 = 3; }"},
      {"for (let [var1, var2] of [[1, 1], [2, 2]]) { var2 = 3; }"},
      {"for (const [var1, var2] of [[1, 1], [2, 2]]) { var2 = 3; }"},

      {"for ([var1, var2] of [[1, 1], [2, 2]]) { () => { var2 = 3; } }"},
      {"for (var [var1, var2] of [[1, 1], [2, 2]]) { () => { var2 = 3; } }"},
      {"for (let [var1, var2] of [[1, 1], [2, 2]]) { () => { var2 = 3; } }"},
      {"for (const [var1, var2] of [[1, 1], [2, 2]]) { () => { var2 = 3; } }"},

      // Skippable function in loop header
      {"for (let [var1, var2 = function() { }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var1; }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var2; }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var1; var2; }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var1 = 0; }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var2 = 0; }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var1 = 0; var2 = 0; }] of [[1]]) { "
       "}"},

      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var1; } }"},
      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var2; } }"},
      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var1; var2; } }"},
      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var1 = 0; } }"},
      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var2 = 0; } }"},
      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var1 = 0; var2 = 0; } }"},
      {"for (let [var1, var2 = function() { var1; }] of [[1]]) { "
       "function f() { var1; } }"},
      {"for (let [var1, var2 = function() { var1; }] of [[1]]) { "
       "function f() { var2; } }"},
      {"for (let [var1, var2 = function() { var1; }] of [[1]]) { "
       "function f() { var1; var2; } }"},
      {"for (let [var1, var2 = function() { var2; }] of [[1]]) { "
       "function f() { var1; } }"},
      {"for (let [var1, var2 = function() { var2; }] of [[1]]) { "
       "function f() { var2; } }"},
      {"for (let [var1, var2 = function() { var2; }] of [[1]]) { "
       "function f() { var1; var2; } }"},

      // Loops without declarations
      {"var var1 = 0; for ( ; var1 < 2; ++var1) { }"},
      {"var var1 = 0; for ( ; var1 < 2; ++var1) { function foo() { var1; } }"},
      {"var var1 = 0; for ( ; var1 > 2; ) { }"},
      {"var var1 = 0; for ( ; var1 > 2; ) { function foo() { var1; } }"},
      {"var var1 = 0; for ( ; var1 > 2; ) { function foo() { var1 = 6; } }"},

      {"var var1 = 0; for(var1; var1 < 2; ++var1) { }"},
      {"var var1 = 0; for (var1; var1 < 2; ++var1) { function foo() { var1; } "
       "}"},
      {"var var1 = 0; for (var1; var1 > 2; ) { }"},
      {"var var1 = 0; for (var1; var1 > 2; ) { function foo() { var1; } }"},
      {"var var1 = 0; for (var1; var1 > 2; ) { function foo() { var1 = 6; } }"},

      // Block functions (potentially sloppy).
      {"if (true) { function f1() {} }"},
      {"if (true) { function f1() {} function f1() {} }", SKIP_STRICT},
      {"if (true) { if (true) { function f1() {} } }"},
      {"if (true) { if (true) { function f1() {} function f1() {} } }",
       SKIP_STRICT},
      {"if (true) { function f1() {} f1 = 3; }"},

      {"if (true) { function f1() {} function foo() { f1; } }"},
      {"if (true) { function f1() {} } function foo() { f1; }"},
      {"if (true) { function f1() {} function f1() {} function foo() { f1; } "
       "}",
       SKIP_STRICT},
      {"if (true) { function f1() {} function f1() {} } function foo() { f1; "
       "}",
       SKIP_STRICT},
      {"if (true) { if (true) { function f1() {} } function foo() { f1; } }"},
      {"if (true) { if (true) { function f1() {} function f1() {} } function "
       "foo() { f1; } }",
       SKIP_STRICT},
      {"if (true) { function f1() {} f1 = 3; function foo() { f1; } }"},
      {"if (true) { function f1() {} f1 = 3; } function foo() { f1; }"},

      {"var f1 = 1; if (true) { function f1() {} }"},
      {"var f1 = 1; if (true) { function f1() {} } function foo() { f1; }"},

      {"if (true) { function f1() {} function f2() { f1(); } }"},

      {"if (true) { function *f1() {} }"},
      {"if (true) { async function f1() {} }"},

      // (Potentially sloppy) block function shadowing a catch variable.
      {"try { } catch(var1) { if (true) { function var1() {} } }"},

      // Simple parameters.
      {"var1", ""},
      {"var1", "var1;"},
      {"var1", "var1 = 9;"},
      {"var1", "function f1() { var1; }"},
      {"var1", "function f1() { var1 = 9; }"},

      {"var1, var2", ""},
      {"var1, var2", "var2;"},
      {"var1, var2", "var2 = 9;"},
      {"var1, var2", "function f1() { var2; }"},
      {"var1, var2", "function f1() { var2 = 9; }"},
      {"var1, var2", "var1;"},
      {"var1, var2", "var1 = 9;"},
      {"var1, var2", "function f1() { var1; }"},
      {"var1, var2", "function f1() { var1 = 9; }"},

      // Duplicate parameters.
      {"var1, var1", "", SkipTests(SKIP_STRICT | SKIP_ARROW)},
      {"var1, var1", "var1;", SkipTests(SKIP_STRICT | SKIP_ARROW)},
      {"var1, var1", "var1 = 9;", SkipTests(SKIP_STRICT | SKIP_ARROW)},
      {"var1, var1", "function f1() { var1; }",
       SkipTests(SKIP_STRICT | SKIP_ARROW)},
      {"var1, var1", "function f1() { var1 = 9; }",
       SkipTests(SKIP_STRICT | SKIP_ARROW)},

      // If the function declares itself strict, non-simple parameters aren't
      // allowed.

      // Rest parameter.
      {"...var2", "", SKIP_STRICT_FUNCTION},
      {"...var2", "var2;", SKIP_STRICT_FUNCTION},
      {"...var2", "var2 = 9;", SKIP_STRICT_FUNCTION},
      {"...var2", "function f1() { var2; }", SKIP_STRICT_FUNCTION},
      {"...var2", "function f1() { var2 = 9; }", SKIP_STRICT_FUNCTION},

      {"var1, ...var2", "", SKIP_STRICT_FUNCTION},
      {"var1, ...var2", "var2;", SKIP_STRICT_FUNCTION},
      {"var1, ...var2", "var2 = 9;", SKIP_STRICT_FUNCTION},
      {"var1, ...var2", "function f1() { var2; }", SKIP_STRICT_FUNCTION},
      {"var1, ...var2", "function f1() { var2 = 9; }", SKIP_STRICT_FUNCTION},

      // Default parameters.
      {"var1 = 3", "", SKIP_STRICT_FUNCTION, PreciseMaybeAssigned::NO},
      {"var1, var2 = var1", "", SKIP_STRICT_FUNCTION, PreciseMaybeAssigned::NO},
      {"var1, var2 = 4, ...var3", "", SKIP_STRICT_FUNCTION,
       PreciseMaybeAssigned::NO},

      // Destructuring parameters. Because of the search space explosion, we
      // cannot test all interesting cases. Let's try to test a relevant subset.
      {"[]", "", SKIP_STRICT_FUNCTION},
      {"{}", "", SKIP_STRICT_FUNCTION},

      {"[var1]", "", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "", SKIP_STRICT_FUNCTION},
      {"{var1}", "", SKIP_STRICT_FUNCTION},

      {"[var1]", "var1;", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "var1;", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "name1;", SKIP_STRICT_FUNCTION},
      {"{var1}", "var1;", SKIP_STRICT_FUNCTION},

      {"[var1]", "var1 = 16;", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "var1 = 16;", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "name1 = 16;", SKIP_STRICT_FUNCTION},
      {"{var1}", "var1 = 16;", SKIP_STRICT_FUNCTION},

      {"[var1]", "() => { var1; };", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "() => { var1; };", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "() => { name1; };", SKIP_STRICT_FUNCTION},
      {"{var1}", "() => { var1; };", SKIP_STRICT_FUNCTION},

      {"[var1, var2, var3]", "", SKIP_STRICT_FUNCTION},
      {"{name1: var1, name2: var2, name3: var3}", "", SKIP_STRICT_FUNCTION},
      {"{var1, var2, var3}", "", SKIP_STRICT_FUNCTION},

      {"[var1, var2, var3]", "() => { var2 = 16;};", SKIP_STRICT_FUNCTION},
      {"{name1: var1, name2: var2, name3: var3}", "() => { var2 = 16;};",
       SKIP_STRICT_FUNCTION},
      {"{name1: var1, name2: var2, name3: var3}", "() => { name2 = 16;};",
       SKIP_STRICT_FUNCTION},
      {"{var1, var2, var3}", "() => { var2 = 16;};", SKIP_STRICT_FUNCTION},

      // Nesting destructuring.
      {"[var1, [var2, var3], {var4, name5: [var5, var6]}]", "",
       SKIP_STRICT_FUNCTION},

      // Complicated params.
      {"var1, [var2], var3 = 24, [var4, var5] = [2, 4], var6, {var7}, var8, "
       "{name9: var9, name10: var10}, ...var11",
       "", SKIP_STRICT_FUNCTION, PreciseMaybeAssigned::NO},

      // Complicated cases from bugs.
      {"var1 = {} = {}", "", SKIP_STRICT_FUNCTION, PreciseMaybeAssigned::NO},

      // Destructuring rest. Because we can.
      {"var1, ...[var2]", "", SKIP_STRICT_FUNCTION},
      {"var1, ...[var2]", "() => { var2; };", SKIP_STRICT_FUNCTION},
      {"var1, ...{0: var2}", "", SKIP_STRICT_FUNCTION},
      {"var1, ...{0: var2}", "() => { var2; };", SKIP_STRICT_FUNCTION},
      {"var1, ...[]", "", SKIP_STRICT_FUNCTION},
      {"var1, ...{}", "", SKIP_STRICT_FUNCTION},
      {"var1, ...[var2, var3]", "", SKIP_STRICT_FUNCTION},
      {"var1, ...{0: var2, 1: var3}", "", SKIP_STRICT_FUNCTION},

      // Default parameters for destruring parameters.
      {"[var1, var2] = [2, 4]", "", SKIP_STRICT_FUNCTION,
       PreciseMaybeAssigned::NO},
      {"{var1, var2} = {var1: 3, var2: 3}", "", SKIP_STRICT_FUNCTION,
       PreciseMaybeAssigned::NO},

      // Default parameters inside destruring parameters.
      {"[var1 = 4, var2 = var1]", "", SKIP_STRICT_FUNCTION,
       PreciseMaybeAssigned::NO},
      {"{var1 = 4, var2 = var1}", "", SKIP_STRICT_FUNCTION,
       PreciseMaybeAssigned::NO},

      // Locals shadowing parameters.
      {"var1, var2", "var var1 = 16; () => { var1 = 17; };"},

      // Locals shadowing destructuring parameters and the rest parameter.
      {"[var1, var2]", "var var1 = 16; () => { var1 = 17; };",
       SKIP_STRICT_FUNCTION},
      {"{var1, var2}", "var var1 = 16; () => { var1 = 17; };",
       SKIP_STRICT_FUNCTION},
      {"var1, var2, ...var3", "var var3 = 16; () => { var3 = 17; };",
       SKIP_STRICT_FUNCTION},
      {"var1, var2 = var1", "var var1 = 16; () => { var1 = 17; };",
       SKIP_STRICT_FUNCTION, PreciseMaybeAssigned::NO},

      // Hoisted sloppy block function shadowing a parameter.
      // FIXME(marja): why is maybe_assigned inaccurate?
      {"var1, var2", "for (;;) { function var1() { } }", DONT_SKIP,
       PreciseMaybeAssigned::NO},

      // Sloppy eval in default parameter.
      {"var1, var2 = eval(''), var3", "let var4 = 0;", SKIP_STRICT_FUNCTION,
       Bailout::BAILOUT_IF_OUTER_SLOPPY},
      {"var1, var2 = eval(''), var3 = eval('')", "let var4 = 0;",
       SKIP_STRICT_FUNCTION, Bailout::BAILOUT_IF_OUTER_SLOPPY},

      // Sloppy eval in arrow function parameter list which is inside another
      // arrow function parameter list.
      {"var1, var2 = (var3, var4 = eval(''), var5) => { let var6; }, var7",
       "let var8 = 0;", SKIP_STRICT_FUNCTION, Bailout::BAILOUT_IF_OUTER_SLOPPY},

      // Sloppy eval in a function body with non-simple parameters.
      {"var1 = 1, var2 = 2", "eval('');", SKIP_STRICT_FUNCTION},

      // Catch variable
      {"try { } catch(var1) { }"},
      {"try { } catch(var1) { var1; }"},
      {"try { } catch(var1) { var1 = 3; }"},
      {"try { } catch(var1) { function f() { var1; } }"},
      {"try { } catch(var1) { function f() { var1 = 3; } }"},

      {"try { } catch({var1, var2}) { function f() { var1 = 3; } }"},
      {"try { } catch([var1, var2]) { function f() { var1 = 3; } }"},
      {"try { } catch({}) { }"},
      {"try { } catch([]) { }"},

      // Shadowing the catch variable
      {"try { } catch(var1) { var var1 = 3; }"},
      {"try { } catch(var1) { var var1 = 3; function f() { var1 = 3; } }"},

      // Classes
      {"class MyClass {}"},
      {"var1 = class MyClass {};"},
      {"var var1 = class MyClass {};"},
      {"let var1 = class MyClass {};"},
      {"const var1 = class MyClass {};"},
      {"var var1 = class {};"},
      {"let var1 = class {};"},
      {"const var1 = class {};"},

      {"class MyClass { constructor() {} }"},
      {"class MyClass { constructor() { var var1; } }"},
      {"class MyClass { constructor() { var var1 = 11; } }"},
      {"class MyClass { constructor() { var var1; function foo() { var1 = 11; "
       "} } }"},

      {"class MyClass { m() {} }"},
      {"class MyClass { m() { var var1; } }"},
      {"class MyClass { m() { var var1 = 11; } }"},
      {"class MyClass { m() { var var1; function foo() { var1 = 11; } } }"},

      {"class MyClass { static m() {} }"},
      {"class MyClass { static m() { var var1; } }"},
      {"class MyClass { static m() { var var1 = 11; } }"},
      {"class MyClass { static m() { var var1; function foo() { var1 = 11; } } "
       "}"},

      {"class MyBase {} class MyClass extends MyBase {}"},
      {"class MyClass extends MyBase { constructor() {} }"},
      {"class MyClass extends MyBase { constructor() { super(); } }"},
      {"class MyClass extends MyBase { constructor() { var var1; } }"},
      {"class MyClass extends MyBase { constructor() { var var1 = 11; } }"},
      {"class MyClass extends MyBase { constructor() { var var1; function "
       "foo() { var1 = 11; } } }"},

      {"class MyClass extends MyBase { m() {} }"},
      {"class MyClass extends MyBase { m() { super.foo; } }"},
      {"class MyClass extends MyBase { m() { var var1; } }"},
      {"class MyClass extends MyBase { m() { var var1 = 11; } }"},
      {"class MyClass extends MyBase { m() { var var1; function foo() { var1 = "
       "11; } } }"},

      {"class MyClass extends MyBase { static m() {} }"},
      {"class MyClass extends MyBase { static m() { super.foo; } }"},
      {"class MyClass extends MyBase { static m() { var var1; } }"},
      {"class MyClass extends MyBase { static m() { var var1 = 11; } }"},
      {"class MyClass extends MyBase { static m() { var var1; function foo() { "
       "var1 = 11; } } }"},

      {"class X { ['bar'] = 1; }; new X;",
       [] { i::FLAG_harmony_public_fields = true; },
       [] { i::FLAG_harmony_public_fields = false; }},
      {"class X { static ['foo'] = 2; }; new X;",
       [] {
         i::FLAG_harmony_public_fields = true;
         i::FLAG_harmony_static_fields = true;
       },
       [] {
         i::FLAG_harmony_public_fields = false;
         i::FLAG_harmony_static_fields = false;
       }},
      {"class X { ['bar'] = 1; static ['foo'] = 2; }; new X;",
       [] {
         i::FLAG_harmony_public_fields = true;
         i::FLAG_harmony_static_fields = true;
       },
       [] {
         i::FLAG_harmony_public_fields = false;
         i::FLAG_harmony_static_fields = false;
       }},
      {"class X { #x = 1 }; new X;",
       [] { i::FLAG_harmony_private_fields = true; },
       [] { i::FLAG_harmony_private_fields = false; }},
      {"function t() { return class { #x = 1 }; } new t();",
       [] { i::FLAG_harmony_private_fields = true; },
       [] { i::FLAG_harmony_private_fields = false; }},
  };

  for (unsigned outer_ix = 0; outer_ix < arraysize(outers); ++outer_ix) {
    for (unsigned inner_ix = 0; inner_ix < arraysize(inners); ++inner_ix) {
      if (outers[outer_ix].strict_outer &&
          (inners[inner_ix].skip & SKIP_STRICT_OUTER)) {
        continue;
      }
      if (outers[outer_ix].strict_test_function &&
          (inners[inner_ix].skip & SKIP_STRICT_FUNCTION)) {
        continue;
      }
      if (outers[outer_ix].arrow && (inners[inner_ix].skip & SKIP_ARROW)) {
        continue;
      }

      const char* code = outers[outer_ix].code;
      int code_len = Utf8LengthHelper(code);

      int params_len = Utf8LengthHelper(inners[inner_ix].params);
      int source_len = Utf8LengthHelper(inners[inner_ix].source);
      int len = code_len + params_len + source_len;

      if (inners[inner_ix].prologue != nullptr) {
        inners[inner_ix].prologue();
      }

      i::ScopedVector<char> program(len + 1);
      i::SNPrintF(program, code, inners[inner_ix].params,
                  inners[inner_ix].source);

      i::HandleScope scope(isolate);

      i::Handle<i::String> source =
          factory->InternalizeUtf8String(program.start());
      source->PrintOn(stdout);
      printf("\n");

      // Compile and run the script to get a pointer to the lazy function.
      v8::Local<v8::Value> v = CompileRun(program.start());
      i::Handle<i::Object> o = v8::Utils::OpenHandle(*v);
      i::Handle<i::JSFunction> f = i::Handle<i::JSFunction>::cast(o);
      i::Handle<i::SharedFunctionInfo> shared = i::handle(f->shared(), isolate);

      if (inners[inner_ix].bailout == Bailout::BAILOUT_IF_OUTER_SLOPPY &&
          !outers[outer_ix].strict_outer) {
        CHECK(!shared->HasUncompiledDataWithPreParsedScope());
        continue;
      }

      CHECK(shared->HasUncompiledDataWithPreParsedScope());
      i::Handle<i::PreParsedScopeData> produced_data_on_heap(
          shared->uncompiled_data_with_pre_parsed_scope()
              ->pre_parsed_scope_data(),
          isolate);

      // Parse the lazy function using the scope data.
      i::ParseInfo using_scope_data(isolate, shared);
      using_scope_data.set_lazy_compile();
      using_scope_data.set_consumed_preparsed_scope_data(
          i::ConsumedPreParsedScopeData::For(isolate, produced_data_on_heap));
      CHECK(i::parsing::ParseFunction(&using_scope_data, shared, isolate));

      // Verify that we skipped at least one function inside that scope.
      i::DeclarationScope* scope_with_skipped_functions =
          using_scope_data.literal()->scope();
      CHECK(i::ScopeTestHelper::HasSkippedFunctionInside(
          scope_with_skipped_functions));

      // Do scope allocation (based on the preparsed scope data).
      CHECK(i::DeclarationScope::Analyze(&using_scope_data));

      // Parse the lazy function again eagerly to produce baseline data.
      i::ParseInfo not_using_scope_data(isolate, shared);
      not_using_scope_data.set_lazy_compile();
      CHECK(i::parsing::ParseFunction(&not_using_scope_data, shared, isolate));

      // Verify that we didn't skip anything (there's no preparsed scope data,
      // so we cannot skip).
      i::DeclarationScope* scope_without_skipped_functions =
          not_using_scope_data.literal()->scope();
      CHECK(!i::ScopeTestHelper::HasSkippedFunctionInside(
          scope_without_skipped_functions));

      // Do normal scope allocation.
      CHECK(i::DeclarationScope::Analyze(&not_using_scope_data));

      // Verify that scope allocation gave the same results when parsing w/ the
      // scope data (and skipping functions), and when parsing without.
      i::ScopeTestHelper::CompareScopes(
          scope_without_skipped_functions, scope_with_skipped_functions,
          inners[inner_ix].precise_maybe_assigned == PreciseMaybeAssigned::YES);

      if (inners[inner_ix].epilogue != nullptr) {
        inners[inner_ix].epilogue();
      }
    }
  }
}

// Regression test for
// https://bugs.chromium.org/p/chromium/issues/detail?id=753896. Should not
// crash.
TEST(Regress753896) {
  i::FLAG_preparser_scope_analysis = true;
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  i::HandleScope scope(isolate);
  LocalContext env;

  i::Handle<i::String> source = factory->InternalizeUtf8String(
      "function lazy() { let v = 0; if (true) { var v = 0; } }");
  i::Handle<i::Script> script = factory->NewScript(source);
  i::ParseInfo info(isolate, script);

  // We don't assert that parsing succeeded or that it failed; currently the
  // error is not detected inside lazy functions, but it might be in the future.
  i::parsing::ParseProgram(&info, isolate);
}

TEST(ProducingAndConsumingByteData) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);
  LocalContext env;

  i::Zone zone(isolate->allocator(), ZONE_NAME);
  i::PreParsedScopeDataBuilder::ByteData bytes(&zone);
  // Write some data.
  bytes.WriteUint32(1983);  // This will be overwritten.
  bytes.WriteUint32(2147483647);
  bytes.WriteUint8(4);
  bytes.WriteUint8(255);
  bytes.WriteUint32(0);
  bytes.WriteUint8(0);
#ifdef DEBUG
  bytes.OverwriteFirstUint32(2017);
#endif
  bytes.WriteUint8(100);
  // Write quarter bytes between uint8s and uint32s to verify they're stored
  // correctly.
  bytes.WriteQuarter(3);
  bytes.WriteQuarter(0);
  bytes.WriteQuarter(2);
  bytes.WriteQuarter(1);
  bytes.WriteQuarter(0);
  bytes.WriteUint8(50);
  bytes.WriteQuarter(0);
  bytes.WriteQuarter(1);
  bytes.WriteQuarter(2);
  bytes.WriteUint32(50);
  // End with a lonely quarter.
  bytes.WriteQuarter(2);

  {
    // Serialize as a ZoneConsumedPreParsedScopeData, and read back data.
    i::ZonePreParsedScopeData zone_serialized(&zone, bytes.begin(), bytes.end(),
                                              0);
    i::ZoneConsumedPreParsedScopeData::ByteData bytes_for_reading;
    i::ZoneVectorWrapper wrapper(zone_serialized.byte_data());
    i::ZoneConsumedPreParsedScopeData::ByteData::ReadingScope reading_scope(
        &bytes_for_reading, &wrapper);

#ifdef DEBUG
    CHECK_EQ(bytes_for_reading.ReadUint32(), 2017);
#else
    CHECK_EQ(bytes_for_reading.ReadUint32(), 1983);
#endif
    CHECK_EQ(bytes_for_reading.ReadUint32(), 2147483647);
    CHECK_EQ(bytes_for_reading.ReadUint8(), 4);
    CHECK_EQ(bytes_for_reading.ReadUint8(), 255);
    CHECK_EQ(bytes_for_reading.ReadUint32(), 0);
    CHECK_EQ(bytes_for_reading.ReadUint8(), 0);
    CHECK_EQ(bytes_for_reading.ReadUint8(), 100);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 3);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 0);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 2);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 1);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 0);
    CHECK_EQ(bytes_for_reading.ReadUint8(), 50);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 0);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 1);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 2);
    CHECK_EQ(bytes_for_reading.ReadUint32(), 50);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 2);
  }

  {
    // Serialize as an OnHeapConsumedPreParsedScopeData, and read back data.
    i::Handle<i::PodArray<uint8_t>> data_on_heap = bytes.Serialize(isolate);
    i::OnHeapConsumedPreParsedScopeData::ByteData bytes_for_reading;
    i::OnHeapConsumedPreParsedScopeData::ByteData::ReadingScope reading_scope(
        &bytes_for_reading, *data_on_heap);

#ifdef DEBUG
    CHECK_EQ(bytes_for_reading.ReadUint32(), 2017);
#else
    CHECK_EQ(bytes_for_reading.ReadUint32(), 1983);
#endif
    CHECK_EQ(bytes_for_reading.ReadUint32(), 2147483647);
    CHECK_EQ(bytes_for_reading.ReadUint8(), 4);
    CHECK_EQ(bytes_for_reading.ReadUint8(), 255);
    CHECK_EQ(bytes_for_reading.ReadUint32(), 0);
    CHECK_EQ(bytes_for_reading.ReadUint8(), 0);
    CHECK_EQ(bytes_for_reading.ReadUint8(), 100);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 3);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 0);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 2);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 1);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 0);
    CHECK_EQ(bytes_for_reading.ReadUint8(), 50);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 0);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 1);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 2);
    CHECK_EQ(bytes_for_reading.ReadUint32(), 50);
    CHECK_EQ(bytes_for_reading.ReadQuarter(), 2);
  }
}
