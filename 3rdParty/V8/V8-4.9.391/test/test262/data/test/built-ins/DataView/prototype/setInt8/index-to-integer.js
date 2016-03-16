// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.2.4.15
description: >
  The requested index is converted with ToInteger.
info: >
  ...
  3. Return SetViewValue(v, byteOffset, littleEndian, "Int8", value).

  24.2.1.2 SetViewValue ( view, requestIndex, isLittleEndian, type, value )
    ...
    3. Let numberIndex be ToNumber(requestIndex).
    4. Let getIndex be ToInteger(numberIndex).
    5. ReturnIfAbrupt(getIndex).
    ...
---*/

var dataView = new DataView(new ArrayBuffer(8));

dataView.setInt8(+0, 1);
assert.sameValue(dataView.getInt8(0), 1, "setInt8(+0, 1)");

dataView.setInt8(-0, 2);
assert.sameValue(dataView.getInt8(0), 2, "setInt8(-0, 2)");

dataView.setInt8(1, 3);
assert.sameValue(dataView.getInt8(1), 3, "setInt8(1, 3)");

dataView.setInt8(null, 4);
assert.sameValue(dataView.getInt8(0), 4, "setInt8(null, 4)");

dataView.setInt8(false, 5);
assert.sameValue(dataView.getInt8(0), 5, "setInt8(false, 5)");

dataView.setInt8(true, 6);
assert.sameValue(dataView.getInt8(1), 6, "setInt8(true, 6)");

dataView.setInt8("", 7);
assert.sameValue(dataView.getInt8(0), 7, "setInt8('', 7)");

// Math.pow(2, 31) = 2147483648
assert.throws(RangeError, function() {
  dataView.setInt8(2147483648, 8);
}, "setInt8(2147483648, 8)");

// Math.pow(2, 32) = 4294967296
assert.throws(RangeError, function() {
  dataView.setInt8(4294967296, 9);
}, "setInt8(4294967296, 9)");

var obj = {
  valueOf: function() {
    return 1;
  }
};
dataView.setInt8(obj, 10);
assert.sameValue(dataView.getInt8(1), 10, "setInt8(obj, 10)");
