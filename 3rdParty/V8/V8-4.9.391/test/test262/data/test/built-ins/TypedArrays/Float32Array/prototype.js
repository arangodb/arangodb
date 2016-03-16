// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.5.2
description: >
  The initial value of Float32Array.prototype is the Float32Array prototype object.
info: >
  The initial value of TypedArray.prototype is the corresponding TypedArray prototype intrinsic object (22.2.6).

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Float32Array.prototype, Object.getPrototypeOf(new Float32Array(0)));

verifyNotEnumerable(Float32Array, "prototype");
verifyNotWritable(Float32Array, "prototype");
verifyNotConfigurable(Float32Array, "prototype");
