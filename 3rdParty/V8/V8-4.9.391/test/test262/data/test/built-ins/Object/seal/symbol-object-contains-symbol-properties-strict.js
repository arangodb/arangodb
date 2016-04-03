// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.4
description: >
    Object.seal(obj) where obj contains symbol properties.
flags: [onlyStrict]
---*/
var symA = Symbol("A");
var symB = Symbol("B");
var obj = {};
obj[symA] = 1;
Object.seal(obj);
obj[symA] = 2;

assert.sameValue(obj[symA], 2, "The value of `obj[symA]` is `2`");

assert.throws(TypeError, function() {
  delete obj[symA];
});

assert.throws(TypeError, function() {
  obj[symB] = 1;
});

