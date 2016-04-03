// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.2.12
description: >
  Replace default values, unless argument is undefined
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
features: [Symbol]
---*/

function fn(a = 1, b = 2, c = 3) {
  return [a, b, c];
}

var results = fn('', 'foo', 'undefined');
assert.sameValue(results[0], '', 'empty string replace default value');
assert.sameValue(results[1], 'foo', 'string replaces default value');
assert.sameValue(
  results[2], 'undefined',
  '"undefined" string replaces default value'
);

results = fn(0, 42, -Infinity);
assert.sameValue(results[0], 0, 'Number (0) replaces default value');
assert.sameValue(results[1], 42, 'number replaces default value');
assert.sameValue(results[2], -Infinity, '-Infinity replaces default value');

var o = {};
var arr = [];
results = fn(o, arr, null);
assert.sameValue(results[0], o, 'object replaces default value');
assert.sameValue(results[1], arr, 'array replaces default value');
assert.sameValue(results[2], null, 'null replaces default value');

var s = Symbol('');
results = fn(true, false, s);
assert.sameValue(results[0], true, 'boolean true replaces default value');
assert.sameValue(results[1], false, 'boolean false replaces default value');
assert.sameValue(results[2], s, 'Symbol replaces default value');

results = fn(undefined, NaN, undefined);
assert.sameValue(results[0], 1, 'undefined argument does not replace default');
assert.sameValue(results[1], NaN, 'NaN replaces default value');
assert.sameValue(results[2], 3, 'undefined argument does not replace default');
