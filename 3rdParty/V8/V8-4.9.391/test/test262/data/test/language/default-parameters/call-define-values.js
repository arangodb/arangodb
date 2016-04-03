// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.2.12
description: >
  Function call define default values to arguments
info: >
  9.2.12 FunctionDeclarationInstantiation(func, argumentsList)

  24. If hasDuplicates is true, then
  ...
  25. Else,
    a. Let formalStatus be IteratorBindingInitialization for formals with
       iteratorRecord and env as arguments.

  ES6 13.3.3.6 Runtime Semantics: IteratorBindingInitialization

  SingleNameBinding : BindingIdentifier Initializeropt

  ...
  6. If Initializer is present and v is undefined, then
    a. Let defaultValue be the result of evaluating Initializer.
    b. Let v be GetValue(defaultValue).
    ...
---*/

var results;

var o = {};

function fn(a = 1, b = null, c = o, d) {
  return [a, b, c, d];
}

results = fn();

assert.sameValue(results[0], 1, 'apply default values #1');
assert.sameValue(results[1], null, 'apply default values #2');
assert.sameValue(results[2], o, 'apply default values #3');

assert.sameValue(
  results[3], undefined,
  'Parameters without defaults after default parameters defaults to undefined'
);
