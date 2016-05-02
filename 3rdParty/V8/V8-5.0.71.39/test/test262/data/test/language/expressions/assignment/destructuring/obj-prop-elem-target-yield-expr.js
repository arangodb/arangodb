// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When a `yield` token appears within the DestructuringAssignmentTarget of an
    AssignmentElement and within a generator function body, it should behave as
    a YieldExpression.
es6id: 12.14.5.4
features: [generators]
---*/

var value = { x: 23 };
var x = {};
var assignmentResult, iterationResult, iter;

iter = (function*() {
  assignmentResult = { x: x[yield] } = value;
}());

iterationResult = iter.next();

assert.sameValue(assignmentResult, undefined);
assert.sameValue(iterationResult.value, undefined);
assert.sameValue(iterationResult.done, false);
assert.sameValue(x.prop, undefined);

iterationResult = iter.next('prop');

assert.sameValue(assignmentResult, value);
assert.sameValue(iterationResult.value, undefined);
assert.sameValue(iterationResult.done, true);
assert.sameValue(x.prop, 23);
