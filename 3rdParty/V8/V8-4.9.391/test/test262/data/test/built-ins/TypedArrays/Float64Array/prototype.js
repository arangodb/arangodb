// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.5.2
description: >
  The initial value of Float64Array.prototype is the Float64Array prototype object.
info: >
  The initial value of TypedArray.prototype is the corresponding TypedArray prototype intrinsic object (22.2.6).

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Float64Array.prototype, Object.getPrototypeOf(new Float64Array(0)));

verifyNotEnumerable(Float64Array, "prototype");
verifyNotWritable(Float64Array, "prototype");
verifyNotConfigurable(Float64Array, "prototype");
