// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.29
description: >
    `this` value not object coercible
info: >
    1. Let O be ToObject(this value).
    2. ReturnIfAbrupt(O).
---*/

assert.throws(TypeError, function() {
  Array.prototype.values.call(undefined);
});

assert.throws(TypeError, function() {
  Array.prototype.values.call(null);
});
