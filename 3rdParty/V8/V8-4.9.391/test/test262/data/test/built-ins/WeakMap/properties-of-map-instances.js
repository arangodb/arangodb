// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.4
description: >
  WeakMap instances are ordinary objects that inherit properties from the
  WeakMap prototype.
---*/

assert.sameValue(
  Object.getPrototypeOf(new WeakMap()),
  WeakMap.prototype,
  '`Object.getPrototypeOf(new WeakMap())` returns `WeakMap.prototype`'
);
