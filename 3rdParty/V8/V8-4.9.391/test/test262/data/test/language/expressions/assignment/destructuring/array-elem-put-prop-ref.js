// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    The DestructuringAssignmentTarget of an AssignmentElement may be a
    PropertyReference.
es6id: 12.14.5.3
---*/

var value = [4];
var x = {};
var result;

result = [x.y] = value;

assert.sameValue(result, value);
assert.sameValue(x.y, 4);
