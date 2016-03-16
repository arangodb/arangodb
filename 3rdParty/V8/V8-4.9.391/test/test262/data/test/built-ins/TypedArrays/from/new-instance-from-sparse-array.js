// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.from
description: >
  Return a new TypedArray from a sparse array
includes: [testTypedArray.js]
features: [Array.prototype.values]
---*/

var source = [,,42,,44,,];

testWithTypedArrayConstructors(function(TA) {
  var result = TA.from(source);

  assert.sameValue(result.length, 6);
  assert.sameValue(result[0], NaN);
  assert.sameValue(result[1], NaN);
  assert.sameValue(result[2], 42);
  assert.sameValue(result[3], NaN);
  assert.sameValue(result[4], 44);
  assert.sameValue(result[5], NaN);
  assert.sameValue(result.constructor, TA);
  assert.sameValue(Object.getPrototypeOf(result), TA.prototype);
},
[
  Float32Array,
  Float64Array
]);

testWithTypedArrayConstructors(function(TA) {
  var result = TA.from(source);

  assert.sameValue(result.length, 6);
  assert.sameValue(result[0], 0);
  assert.sameValue(result[1], 0);
  assert.sameValue(result[2], 42);
  assert.sameValue(result[3], 0);
  assert.sameValue(result[4], 44);
  assert.sameValue(result[5], 0);
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
