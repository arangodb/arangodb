// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.3
description: >
  Throws a TypeError if `this` does not have a [[WeakMapData]] internal slot.
info: >
  WeakMap.prototype.get ( key )

  ...
  3. If M does not have a [[WeakMapData]] internal slot, throw a TypeError
  exception.
  ...
---*/
var map = new WeakMap();

assert.throws(TypeError, function() {
  WeakMap.prototype.get.call([], 1);
});

assert.throws(TypeError, function() {
  map.get.call([], 1);
});

assert.throws(TypeError, function() {
  WeakMap.prototype.get.call({}, 1);
});

assert.throws(TypeError, function() {
  map.get.call({}, 1);
});
