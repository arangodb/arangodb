// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    In the presense of an AssignmentRestElement, value iteration exhausts the
    iterable value;
es6id: 12.14.5.3
features: [generators]
---*/

var count = 0;
var g = function*() {
  count += 1;
  yield;
  count += 1;
  yield;
  count += 1;
}
var iter, result, x;

iter = g();
result = [...x] = iter;

assert.sameValue(result, iter);
assert.sameValue(count, 3);
