// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When DestructuringAssignmentTarget is an array literal and the iterable is
    emits no values, an empty array should be used as the value of the nested
    DestructuringAssignment.
es6id: 12.14.5.3
---*/

var value = [];
var result, x;

result = [...[x]] = value;

assert.sameValue(result, value);
assert.sameValue(x, undefined);
