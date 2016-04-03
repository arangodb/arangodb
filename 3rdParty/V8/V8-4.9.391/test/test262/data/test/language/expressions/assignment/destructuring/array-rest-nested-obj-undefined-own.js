// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When DestructuringAssignmentTarget is an array literal and the iterable
    emits `undefined` as the only value, an array with a single `undefined`
    element should be used as the value of the nested DestructuringAssignment.
es6id: 12.14.5.3
---*/

var value = [undefined];
var result, x, length;

result = [...{ 0: x, length }] = value;

assert.sameValue(result, value);
assert.sameValue(x, undefined);
assert.sameValue(length, 1);
