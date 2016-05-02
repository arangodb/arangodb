// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When DestructuringAssignmentTarget is an array literal, it should be parsed
    parsed as a DestructuringAssignmentPattern and evaluated as a destructuring
    assignment.
es6id: 12.14.5.3
---*/

var value = [1, 2, 3];
var x, result;

result = [...[x]] = value;

assert.sameValue(result, value);
assert.sameValue(x, 1);
