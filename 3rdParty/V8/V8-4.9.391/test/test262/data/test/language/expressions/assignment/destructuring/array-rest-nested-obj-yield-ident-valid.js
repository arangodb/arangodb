// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When a `yield` token appears within the Initializer of a nested
    destructuring assignment and outside of a generator function body, it
    should behave as an IdentifierExpression.
es6id: 12.14.5.3
flags: [noStrict]
---*/

var value = [{}];
var yield = 2;
var result, iterationResult, iter, x;

result = [...{ x = yield }] = value;

assert.sameValue(result, value);
assert.sameValue(x, 2);
