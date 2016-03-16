// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.6
description: >
  The prototype of Uint8Array.prototype is %TypedArrayPrototype%.
info: >
  The value of the [[Prototype]] internal slot of a TypedArray prototype
  object is the intrinsic object %TypedArrayPrototype% (22.2.3).
includes: [testTypedArray.js]
---*/

assert.sameValue(Object.getPrototypeOf(Uint8Array.prototype), TypedArray.prototype);
