// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.2.2.4
description: >
  @@species property returns the `this` value
info: >
  22.2.2.4 get %TypedArray% [ @@species ]

  1. Return the this value.
features: [Symbol.species]
includes: [testTypedArray.js]
---*/

var value = {};
var getter = Object.getOwnPropertyDescriptor(TypedArray, Symbol.species).get;

assert.sameValue(getter.call(value), value);
