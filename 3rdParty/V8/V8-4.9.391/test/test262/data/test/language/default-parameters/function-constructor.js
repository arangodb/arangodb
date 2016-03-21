// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.2.1.1
description: >
  Create new Function with default parameters
---*/

var fn = new Function('a = 42', 'b = "foo"', 'return [a, b]');
var result = fn();

assert.sameValue(result[0], 42);
assert.sameValue(result[1], 'foo');
