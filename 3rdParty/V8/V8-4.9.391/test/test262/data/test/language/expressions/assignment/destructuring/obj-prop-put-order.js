// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    The AssignmentElements in an AssignmentElementList are evaluated in left-
    to-right order.
es6id: 12.14.5.4
---*/

var value = { a: 2, z: 1 };
var x, result;

result = { z: x, a: x } = value;

assert.sameValue(result, value);
assert.sameValue(x, 2);
