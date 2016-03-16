// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.5.2
description: >
  The initial value of Uint16Array.prototype is the Uint16Array prototype object.
info: >
  The initial value of TypedArray.prototype is the corresponding TypedArray prototype intrinsic object (22.2.6).

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Uint16Array.prototype, Object.getPrototypeOf(new Uint16Array(0)));

verifyNotEnumerable(Uint16Array, "prototype");
verifyNotWritable(Uint16Array, "prototype");
verifyNotConfigurable(Uint16Array, "prototype");
