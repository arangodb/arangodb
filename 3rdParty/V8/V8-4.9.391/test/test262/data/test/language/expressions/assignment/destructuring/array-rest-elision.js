// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    ArrayAssignmentPattern may include elisions at any position preceeding a
    AssignmentRestElement in a AssignmentElementList.
es6id: 12.14.5
---*/

var value = [1, 2, 3, 4, 5, 6];
var x, y;
var result;

result = [, , x, , ...y] = value;

assert.sameValue(result, value);
assert.sameValue(x, 3);
assert.sameValue(y.length, 2);
assert.sameValue(y[0], 5);
assert.sameValue(y[1], 6);
