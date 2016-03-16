// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.2.3.2
description: >
  "byteLength" property of TypedArrayPrototype
info: >
  %TypedArray%.prototype.byteLength is an accessor property whose set accessor
  function is undefined.
includes: [testTypedArray.js]
---*/

var TypedArrayPrototype = TypedArray.prototype;
var desc = Object.getOwnPropertyDescriptor(TypedArrayPrototype, 'byteLength');

assert.sameValue(desc.set, undefined);
assert.sameValue(typeof desc.get, 'function');
