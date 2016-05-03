// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When a `yield` token appears within the Initializer of a nested
    destructuring assignment and within a generator function body, it behaves
    as a YieldExpression.
es6id: 12.14.5.3
features: [generators]
---*/

var value = [{}];
var assignmentResult, iterationResult, iter, x;

iter = (function*() {
  assignmentResult = [{ x = yield }] = value;
}());

iterationResult = iter.next();

assert.sameValue(assignmentResult, undefined);
assert.sameValue(iterationResult.value, undefined);
assert.sameValue(iterationResult.done, false);
assert.sameValue(x, undefined);

iterationResult = iter.next(4);

assert.sameValue(assignmentResult, value);
assert.sameValue(iterationResult.value, undefined);
assert.sameValue(iterationResult.done, true);
assert.sameValue(x, 4);
