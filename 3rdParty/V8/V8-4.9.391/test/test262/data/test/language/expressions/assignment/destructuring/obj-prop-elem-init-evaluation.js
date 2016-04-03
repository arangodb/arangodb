// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    The Initializer should only be evaluated if v is undefined.
es6id: 12.14.5.4
---*/

var result, value, x, flag;

value = {};
flag = false;
result = { x: x = flag = true } = value;
assert.sameValue(result, value);
assert.sameValue(x, true);
assert.sameValue(flag, true);

value = { y: 1 };
flag = false;
result = { y: x = flag = true } = value;
assert.sameValue(result, value);
assert.sameValue(x, 1);
assert.sameValue(flag, false);
