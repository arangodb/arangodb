// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 13.3.3.7
description: >
  Return abrupt from a property value as a default argument.
---*/

var obj = {};
Object.defineProperty(obj, 'err', {
  get: function() {
    throw new Test262Error();
  }
});

function fn(a = obj.err) {
  return 42;
}

assert.throws(Test262Error, function() {
  fn();
});
