// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.5.2
description: >
  The initial value of Int32Array.prototype is the Int32Array prototype object.
info: >
  The initial value of TypedArray.prototype is the corresponding TypedArray prototype intrinsic object (22.2.6).

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Int32Array.prototype, Object.getPrototypeOf(new Int32Array(0)));

verifyNotEnumerable(Int32Array, "prototype");
verifyNotWritable(Int32Array, "prototype");
verifyNotConfigurable(Int32Array, "prototype");
