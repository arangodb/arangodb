// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When DestructuringAssignmentTarget is an object literal, it should be
    parsed as a DestructuringAssignmentPattern and evaluated as a destructuring
    assignment.
es6id: 12.14.5.4
---*/

var value = { x: { y: 2 } };
var result, y;

result = { x: { y } } = value;

assert.sameValue(result, value);
assert.sameValue(y, 2);
