// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.2.4.13
description: >
  The requested index is converted with ToInteger.
info: >
  ...
  3. Return SetViewValue(v, byteOffset, littleEndian, "Float32", value).

  24.2.1.2 SetViewValue ( view, requestIndex, isLittleEndian, type, value )
    ...
    3. Let numberIndex be ToNumber(requestIndex).
    4. Let getIndex be ToInteger(numberIndex).
    5. ReturnIfAbrupt(getIndex).
    ...
---*/

var dataView = new DataView(new ArrayBuffer(8));

dataView.setFloat32(+0, 1);
assert.sameValue(dataView.getFloat32(0), 1, "setFloat32(+0, 1)");

dataView.setFloat32(-0, 2);
assert.sameValue(dataView.getFloat32(0), 2, "setFloat32(-0, 2)");

dataView.setFloat32(1, 3);
assert.sameValue(dataView.getFloat32(1), 3, "setFloat32(1, 3)");

dataView.setFloat32(null, 4);
assert.sameValue(dataView.getFloat32(0), 4, "setFloat32(null, 4)");

dataView.setFloat32(false, 5);
assert.sameValue(dataView.getFloat32(0), 5, "setFloat32(false, 5)");

dataView.setFloat32(true, 6);
assert.sameValue(dataView.getFloat32(1), 6, "setFloat32(true, 6)");

dataView.setFloat32("", 7);
assert.sameValue(dataView.getFloat32(0), 7, "setFloat32('', 7)");

// Math.pow(2, 31) = 2147483648
assert.throws(RangeError, function() {
  dataView.setFloat32(2147483648, 8);
}, "setFloat32(2147483648, 8)");

// Math.pow(2, 32) = 4294967296
assert.throws(RangeError, function() {
  dataView.setFloat32(4294967296, 9);
}, "setFloat32(4294967296, 9)");

var obj = {
  valueOf: function() {
    return 1;
  }
};
dataView.setFloat32(obj, 10);
assert.sameValue(dataView.getFloat32(1), 10, "setFloat32(obj, 10)");
