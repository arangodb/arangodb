// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Identifiers that appear as the DestructuringAssignmentTarget in an
    AssignmentElement should take on the iterated value corresponding to their
    position in the ArrayAssignmentPattern.
es6id: 12.14.5.3
---*/

var value = [1, 2, 3];
var x, y, z;
var result;

result = [x, y, z] = value;

assert.sameValue(result, value);
assert.sameValue(x, 1);
assert.sameValue(y, 2);
assert.sameValue(z, 3);
