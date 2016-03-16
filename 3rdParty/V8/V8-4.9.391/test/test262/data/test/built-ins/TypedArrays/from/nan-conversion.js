// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.from
description: >
  Test NaN conversions
info: >
  9.4.5.9 IntegerIndexedElementSet ( O, index, value )

  ...
  3. Let numValue be ? ToNumber(value).
  ...

  24.1.1.6 SetValueInBuffer ( arrayBuffer, byteIndex, type, value [ ,
  isLittleEndian ] )
includes: [testTypedArray.js]
---*/

testWithTypedArrayConstructors(function(TA) {
  var result = TA.from([NaN, undefined]);
  assert.sameValue(result.length, 2);
  assert.sameValue(result[0], NaN);
  assert.sameValue(result[1], NaN);
  assert.sameValue(result.constructor, TA);
  assert.sameValue(Object.getPrototypeOf(result), TA.prototype);
},
[
  Float32Array,
  Float64Array
]);

testWithTypedArrayConstructors(function(TA) {
  var result = TA.from([NaN, undefined]);
  assert.sameValue(result.length, 2);
  assert.sameValue(result[0], 0);
  assert.sameValue(result[1], 0);
  assert.sameValue(result.constructor, TA);
  assert.sameValue(Object.getPrototypeOf(result), TA.prototype);
},
[
  Int8Array,
  Int32Array,
  Int16Array,
  Int8Array,
  Uint32Array,
  Uint16Array,
  Uint8Array,
  Uint8ClampedArray
]);