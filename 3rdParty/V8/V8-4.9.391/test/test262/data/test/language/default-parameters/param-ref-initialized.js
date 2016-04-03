// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.2.12
description: >
  Parameters can refer to previous parameters
---*/

function fn(a = 42, b = a) {
  return b;
}

assert.sameValue(fn(), 42);
assert.sameValue(fn(17), 17);
