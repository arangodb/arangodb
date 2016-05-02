// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.2.12
description: >
  Expressions as parameter default values share the top level context only.
info: >
  9.2.12 FunctionDeclarationInstantiation(func, argumentsList)

  ...
  27. If hasParameterExpressions is false, then
    ...
  28. Else,
    a. NOTE A separate Environment Record is needed to ensure that
       closures created by expressions in the formal parameter list do
       not have visibility of declarations in the function body.
    b. Let varEnv be NewDeclarativeEnvironment(env).
    c. Let varEnvRec be varEnv’s EnvironmentRecord.
    d. Set the VariableEnvironment of calleeContext to varEnv.
---*/

var x = 42;

function fn1(a = x) {
  return a;
}
assert.sameValue(fn1(), 42);

function fn2(a = function() { return x; }) {
  var x = 1;

  return a();
}
assert.sameValue(fn2(), 42);

function fn3(a = function() { var x = 0; }) {
  a();

  return x;
}
assert.sameValue(fn3(), 42);

function fn4(a = y) {
  var y = 1;
}

// y is only defined on the inner scope of fn4
assert.throws(ReferenceError, function() {
  fn4();
});
