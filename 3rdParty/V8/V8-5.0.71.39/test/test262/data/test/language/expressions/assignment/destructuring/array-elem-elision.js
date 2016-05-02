// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    ArrayAssignmentPattern may include elisions at any position in a
    AssignmentElementList that does not contain an AssignmentRestElement.
es6id: 12.14.5
---*/

var value = [1, 2, 3, 4, 5, 6];
var x, y;
var result;

result = [, x, , y, ,] = value;

assert.sameValue(result, value);
assert.sameValue(x, 2);
assert.sameValue(y, 4);
