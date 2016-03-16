// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.5.2
description: >
  The initial value of Uint8ClampedArray.prototype is the Uint8ClampedArray prototype object.
info: >
  The initial value of TypedArray.prototype is the corresponding TypedArray prototype intrinsic object (22.2.6).

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Uint8ClampedArray.prototype, Object.getPrototypeOf(new Uint8ClampedArray(0)));

verifyNotEnumerable(Uint8ClampedArray, "prototype");
verifyNotWritable(Uint8ClampedArray, "prototype");
verifyNotConfigurable(Uint8ClampedArray, "prototype");
