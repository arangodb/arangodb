// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When a `yield` token appears within the Initializer of an
    AssignmentProperty and within a generator function body, it should behave
    as a YieldExpression.
es6id: 12.14.5
features: [generators]
---*/

var value = {};
var assignmentResult, iterationResult, x, iter;

iter = (function*() {
  assignmentResult = { x = yield } = value;
}());

iterationResult = iter.next();

assert.sameValue(iterationResult.value, undefined);
assert.sameValue(iterationResult.done, false);
assert.sameValue(x, undefined);
assert.sameValue(assignmentResult, undefined);

iterationResult = iter.next(3);

assert.sameValue(iterationResult.value, undefined);
assert.sameValue(iterationResult.done, true);
assert.sameValue(x, 3);
assert.sameValue(assignmentResult, value);
