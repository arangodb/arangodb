// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 13.3.3
description: >
  Destructuring values on default parameters.
---*/

function fn([a, b] = [1, 2], {c: c} = {c: 3}) {
  return [a, b, c];
}

var results = fn();

assert.sameValue(results[0], 1);
assert.sameValue(results[1], 2);
assert.sameValue(results[2], 3);
