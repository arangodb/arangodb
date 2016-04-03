// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    PropertyName of an AssignmentProperty may be a ComputedPropertyName.
es6id: 12.14.5.2
---*/

var value = { x: 1, xy: 23, y: 2 };
var result, x, y, xy;

result = { ['x' + 'y']: x } = value;

assert.sameValue(result, value);
assert.sameValue(x, 23);
assert.sameValue(y, undefined);
assert.sameValue(xy, undefined);
