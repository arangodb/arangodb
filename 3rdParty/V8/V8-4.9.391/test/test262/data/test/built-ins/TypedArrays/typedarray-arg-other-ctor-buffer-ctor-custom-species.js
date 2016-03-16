// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-typedarray-typedarray
description: >
  Use default ArrayBuffer constructor on undefined buffer.constructor.@@species
info: >
  22.2.4.3 TypedArray ( typedArray )

  This description applies only if the TypedArray function is called with at
  least one argument and the Type of the first argument is Object and that
  object has a [[TypedArrayName]] internal slot.

  ...
  18. Else,
    a. Let bufferConstructor be ? SpeciesConstructor(srcData, %ArrayBuffer%).
    b. Let data be ? AllocateArrayBuffer(bufferConstructor, byteLength).
  ...

  7.3.20 SpeciesConstructor ( O, defaultConstructor )

  ...
  5. Let S be ? Get(C, @@species).
  6. If S is either undefined or null, return defaultConstructor.
  7. If IsConstructor(S) is true, return S.
  ...

includes: [testTypedArray.js]
features: [Symbol.species]
---*/

var sample1 = new Int8Array();
var sample2 = new Int16Array();

testWithTypedArrayConstructors(function(TA) {
  var sample = TA === Int8Array ? sample2 : sample1;
  var ctor = {};
  var called = 0;
  var custom = {};

  sample.buffer.constructor = ctor;

  ctor[Symbol.species] = function() {
    called++;
  };

  ctor[Symbol.species].prototype = custom;

  var typedArray = new TA(sample);
  assert.sameValue(Object.getPrototypeOf(typedArray.buffer), custom);
  assert.sameValue(called, 0);
});
