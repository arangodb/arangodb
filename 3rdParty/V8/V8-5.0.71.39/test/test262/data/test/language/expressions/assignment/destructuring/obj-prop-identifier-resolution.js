// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Evaluation of DestructuringAssignmentTarget.
es6id: 12.14.5.4
---*/

var result, value, w, x, y;

x = null;
value = { a: 1 };
result = { a: x } = value;

assert.sameValue(result, value);
assert.sameValue(x, 1);

x = null;
value = { a: 2 };
result = { a: x, } = value;

assert.sameValue(result, value);
assert.sameValue(x, 2);

x = null;
value = { a: 3 };
result = { a: x, y } = value;

assert.sameValue(result, value);
assert.sameValue(x, 3);

x = null;
value = { a: 4 };
result = { w, a: x } = value;

assert.sameValue(result, value);
assert.sameValue(x, 4);

x = null;
value = { a: 5 };
result = { w, a: x, y } = value;

assert.sameValue(result, value);
assert.sameValue(x, 5);
