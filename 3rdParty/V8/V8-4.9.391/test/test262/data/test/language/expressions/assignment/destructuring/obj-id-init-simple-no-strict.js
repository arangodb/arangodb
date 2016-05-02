// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Evaluation of DestructuringAssignmentTarget.
es6id: 12.14.5.4
flags: [noStrict]
---*/

var value = {};
var eval, arguments, result;

result = { eval = 3, arguments = 4 } = value;

assert.sameValue(result, value);
assert.sameValue(eval, 3);
assert.sameValue(arguments, 4);
