// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.5
description: >
  The prototype of Uint8Array is %TypedArray%.
info: >
  The value of the [[Prototype]] internal slot of each TypedArray constructor is the %TypedArray% intrinsic object (22.2.1).
includes: [testTypedArray.js]
---*/

assert.sameValue(Object.getPrototypeOf(Uint8Array), TypedArray);
