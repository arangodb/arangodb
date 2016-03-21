// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Evaluation of DestructuringAssignmentTarget.
es6id: 12.14.5.4
flags: [noStrict]
---*/

var value = { eval: 1, arguments: 2 };
var eval, arguments, result;

result = { eval, arguments } = value;

assert.sameValue(result, value);
assert.sameValue(eval, 1);
assert.sameValue(arguments, 2);
