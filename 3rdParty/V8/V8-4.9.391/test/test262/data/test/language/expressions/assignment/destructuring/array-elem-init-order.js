// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Initializer values should be assigned in left-to-right order.
es6id: 12.14.5.3
---*/

var value = [];
var x = 0;
var a, b, result;

result = [ a = x += 1, b = x *= 2 ] = value;

assert.sameValue(result, value);
assert.sameValue(a, 1);
assert.sameValue(b, 2);
assert.sameValue(x, 2);
