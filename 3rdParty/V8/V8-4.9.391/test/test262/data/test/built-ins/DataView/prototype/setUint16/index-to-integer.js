// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.2.4.19
description: >
  The requested index is converted with ToInteger.
info: >
  ...
  3. Return SetViewValue(v, byteOffset, littleEndian, "Uint16", value).

  24.2.1.2 SetViewValue ( view, requestIndex, isLittleEndian, type, value )
    ...
    3. Let numberIndex be ToNumber(requestIndex).
    4. Let getIndex be ToInteger(numberIndex).
    5. ReturnIfAbrupt(getIndex).
    ...
---*/

var dataView = new DataView(new ArrayBuffer(8));

dataView.setUint16(+0, 1);
assert.sameValue(dataView.getUint16(0), 1, "setUint16(+0, 1)");

dataView.setUint16(-0, 2);
assert.sameValue(dataView.getUint16(0), 2, "setUint16(-0, 2)");

dataView.setUint16(1, 3);
assert.sameValue(dataView.getUint16(1), 3, "setUint16(1, 3)");

dataView.setUint16(null, 4);
assert.sameValue(dataView.getUint16(0), 4, "setUint16(null, 4)");

dataView.setUint16(false, 5);
assert.sameValue(dataView.getUint16(0), 5, "setUint16(false, 5)");

dataView.setUint16(true, 6);
assert.sameValue(dataView.getUint16(1), 6, "setUint16(true, 6)");

dataView.setUint16("", 7);
assert.sameValue(dataView.getUint16(0), 7, "setUint16('', 7)");

// Math.pow(2, 31) = 2147483648
assert.throws(RangeError, function() {
  dataView.setUint16(2147483648, 8);
}, "setUint16(2147483648, 8)");

// Math.pow(2, 32) = 4294967296
assert.throws(RangeError, function() {
  dataView.setUint16(4294967296, 9);
}, "setUint16(4294967296, 9)");

var obj = {
  valueOf: function() {
    return 1;
  }
};
dataView.setUint16(obj, 10);
assert.sameValue(dataView.getUint16(1), 10, "setUint16(obj, 10)");
