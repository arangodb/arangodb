// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When a `yield` token appears within the DestructuringAssignmentTarget of a
    nested destructuring assignment and outside of a generator function body,
    it should behave as an IdentifierExpression.
es6id: 12.14.5.3
flags: [noStrict]
---*/

var value = [86];
var yield = 'prop';
var x = {};
var result;

result = [...[x[yield]]] = value;

assert.sameValue(result, value);
assert.sameValue(x.prop, 86);
