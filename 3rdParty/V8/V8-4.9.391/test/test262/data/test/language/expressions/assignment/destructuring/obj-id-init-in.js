// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    The Initializer in an AssignmentProperty may be an `in` expression.
es6id: 12.14.5
---*/

var value = {};
var result, prop;

result = { prop = 'x' in {} } = value;

assert.sameValue(result, value);
assert.sameValue(prop, false);
