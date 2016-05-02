// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.1
description: Throws TypeError if `value` is not Object.
info: >
  WeakSet.prototype.add ( value )

  4. If Type(value) is not Object, throw a TypeError exception.
features: [Symbol]
---*/

var s = new WeakSet();

assert.throws(TypeError, function() {
  s.add(1);
});

assert.throws(TypeError, function() {
  s.add(false);
});

assert.throws(TypeError, function() {
  s.add();
});

assert.throws(TypeError, function() {
  s.add('string');
});

assert.throws(TypeError, function() {
  s.add(null);
});

assert.throws(TypeError, function() {
  s.add(Symbol());
});
