// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    If the Initializer is present and v is undefined, the Initializer should be
    evaluated and the result assigned to the target reference.
es6id: 12.14.5.4
---*/

var result, value, x;

value = {};
result = { x = 1 } = value;

assert.sameValue(result, value);
assert.sameValue(x, 1, 'no property defined');

value = { x: 2 };
result = { x = 1 } = value;

assert.sameValue(result, value);
assert.sameValue(x, 2, 'own property defined (truthy value)');

value = { x: null };
result = { x = 1 } = value;

assert.sameValue(result, value);
assert.sameValue(x, null, 'own property defined (`null`)');

value = { x: undefined };
result = { x = 1 } = value;

assert.sameValue(result, value);
assert.sameValue(x, 1, 'own property defined (`undefined`)');
