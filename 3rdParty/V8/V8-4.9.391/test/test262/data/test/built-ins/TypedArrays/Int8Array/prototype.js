// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.5.2
description: >
  The initial value of Int8Array.prototype is the Int8Array prototype object.
info: >
  The initial value of TypedArray.prototype is the corresponding TypedArray prototype intrinsic object (22.2.6).

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Int8Array.prototype, Object.getPrototypeOf(new Int8Array(0)));

verifyNotEnumerable(Int8Array, "prototype");
verifyNotWritable(Int8Array, "prototype");
verifyNotConfigurable(Int8Array, "prototype");
