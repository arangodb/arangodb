// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When a `yield` token appears within the DestructuringAssignmentTarget of an
    AssignmentRestElement and outside of a generator function body, it should
    behave as an IdentifierReference.
es6id: 12.14.5
flags: [noStrict]
---*/

var value = [33, 44, 55];
var yield = 'prop';
var x = {};
var result;

result = [...x[yield]] = value;

assert.sameValue(result, value);
assert.sameValue(x.prop.length, 3);
assert.sameValue(x.prop[0], 33);
assert.sameValue(x.prop[1], 44);
assert.sameValue(x.prop[2], 55);
