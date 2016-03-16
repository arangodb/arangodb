// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-typedarray-typedarray
description: >
  Return abrupt from buffer.constructor.@@species.prototype
info: >
  22.2.4.3 TypedArray ( typedArray )

  This description applies only if the TypedArray function is called with at
  least one argument and the Type of the first argument is Object and that
  object has a [[TypedArrayName]] internal slot.

  ...
  17. If SameValue(elementType, srcType) is true, then
    a. Let data be ? CloneArrayBuffer(srcData, srcByteOffset).
  ...

  24.1.1.4 CloneArrayBuffer ( srcBuffer, srcByteOffset [ , cloneConstructor ] )

  ...
  2. If cloneConstructor is not present, then
    a. Let cloneConstructor be ? SpeciesConstructor(srcBuffer, %ArrayBuffer%).
  ...

  7.3.20 SpeciesConstructor ( O, defaultConstructor )

  ...
  5. Let S be ? Get(C, @@species).
  6. If S is either undefined or null, return defaultConstructor.
  7. If IsConstructor(S) is true, return S.
  8. Throw a TypeError exception.
includes: [testTypedArray.js]
features: [Symbol.species]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA();
  var ctor = {};
  var m = { m() {} };

  sample.buffer.constructor = ctor;

  ctor[Symbol.species] = m;

  assert.throws(TypeError, function() {
    new TA(sample);
  });
});
