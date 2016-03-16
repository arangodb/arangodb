// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When a `yield` token appears within the DestructuringAssignmentTarget of
    an AssignmentRestElement and within the body of a generator function, it
    should behave as a YieldExpression.
es6id: 12.14.5
features: [generators]
---*/

var value = [33, 44, 55];
var x = {};
var assignmentResult, iterationResult, iter;

iter = (function*() {
  assignmentResult = [...x[yield]] = value;
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
assert.sameValue(x.prop.length, 3);
assert.sameValue(x.prop[0], 33);
assert.sameValue(x.prop[1], 44);
assert.sameValue(x.prop[2], 55);
