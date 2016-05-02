// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    If the Initializer is present and v is undefined, the Initializer should be
    evaluated and the result assigned to the target reference.
es6id: 12.14.5.3
---*/

var result, value, x;

value = [];
result = [ x = 10 ] = value;

assert.sameValue(result, value);
assert.sameValue(x, 10, 'no element at index');

value = [2];
result = [ x = 11 ] = value;

assert.sameValue(result, value);
assert.sameValue(x, 2, 'element at index (truthy value)');

value = [ null ];
result = [ x = 12 ] = value;

assert.sameValue(result, value);
assert.sameValue(x, null, 'element at index (`null`)');

value = [ undefined ];
result = [ x = 13 ] = value;

assert.sameValue(result, value);
assert.sameValue(x, 13, 'element at index (`undefined`)');

value = [ , ];
result = [ x = 14 ] = value;

assert.sameValue(result, value);
assert.sameValue(x, 14, 'element at index (sparse array)');
