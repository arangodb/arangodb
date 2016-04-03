// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.1.2.1
description: Descriptor for `name` property
info: >
    1. Return the this value.
features: [Symbol.iterator]
---*/

var IteratorPrototype = Object.getPrototypeOf(
  Object.getPrototypeOf([][Symbol.iterator]())
);
var thisValue = {};

assert.sameValue(
  IteratorPrototype[Symbol.iterator].call(thisValue),
  thisValue
);
