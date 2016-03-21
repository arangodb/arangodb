// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Value iteration only proceeds for the number of elements in the
    ArrayAssignmentPattern.
es6id: 12.14.5.3
features: [generators]
---*/

var count, iter, result;
var g = function*() {
  count += 1;
  yield;
  count += 1;
  yield;
  count += 1;
}

count = 0;
iter = g();
result = [] = iter;

assert.sameValue(result, iter);
assert.sameValue(count, 0);

iter = g();
result = [,] = iter;

assert.sameValue(result, iter);
assert.sameValue(count, 1);

count = 0;
iter = g();
result = [,,] = iter;

assert.sameValue(result, iter);
assert.sameValue(count, 2);

count = 0;
iter = g();
result = [,,,] = iter;

assert.sameValue(result, iter);
assert.sameValue(count, 3);

count = 0;
iter = g();
result = [,,,,] = iter;

assert.sameValue(result, iter);
assert.sameValue(count, 3);
