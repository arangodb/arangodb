// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es7id: 24.2.4.14
description: >
  Throws a RangeError if the index is negative or non-integral number.
info: >
  ...
  3. Return SetViewValue(v, byteOffset, littleEndian, "Float64", value).

  24.2.1.2 SetViewValue ( view, requestIndex, isLittleEndian, type, value )
    ...
    3. Let numberIndex be ToNumber(requestIndex).
    4. Let getIndex be ? ToInteger(numberIndex).
    5. If numberIndex ≠ getIndex or getIndex < 0, throw a RangeError exception.
    6. Let numberValue be ? ToNumber(value).
    ...
---*/

var dataView = new DataView(new ArrayBuffer(16));

var poisoned = {
  valueOf: function() {
    $ERROR("valueOf called");
  }
};

assert.throws(RangeError, function() {
  dataView.setFloat64(NaN, poisoned);
}, "setFloat64(NaN, poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64(1.5, poisoned);
}, "setFloat64(1.5, poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64(-1.5, poisoned);
}, "setFloat64(-1.5, poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64(-1, poisoned);
}, "setFloat64(-1, poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64(-Infinity, poisoned);
}, "setFloat64(-Infinity, poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64(undefined, poisoned);
}, "setFloat64(undefined, poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64('invalid', poisoned);
}, "setFloat64('invalid', poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64('NaN', poisoned);
}, "setFloat64('NaN', poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64('1.5', poisoned);
}, "setFloat64('1.5', poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64('-1.5', poisoned);
}, "setFloat64('-1.5', poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64('-1', poisoned);
}, "setFloat64('-1', poisoned)");

assert.throws(RangeError, function() {
  dataView.setFloat64('-Infinity', poisoned);
}, "setFloat64('-Infinity', poisoned)");

var obj = {
  valueOf: function() {
    return 1.41421;
  }
};
assert.throws(RangeError, function() {
  dataView.setFloat64(obj, poisoned);
}, "setFloat64(obj, poisoned)");
