// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.2.12
description: >
  Parameters can't refer to not initialized parameters
---*/

function fn1(a = a) {
  return a;
}

assert.throws(ReferenceError, function() {
  fn1();
});


function fn2(a = b, b = 42) {
  return a;
}

assert.throws(ReferenceError, function() {
  fn2();
});
