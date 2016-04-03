// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.1.2.1
description: >
  The `length` parameter is converted to a number value.
info: >
  ArrayBuffer( length )

  ...
  2. Let numberLength be ToNumber(length).
  3. Let byteLength be ToLength(numberLength).
  4. ReturnIfAbrupt(byteLength).
  5. If SameValueZero(numberLength, byteLength) is false, throw a RangeError exception.
  ...
features: [Symbol]
---*/

assert.throws(RangeError, function() {
  new ArrayBuffer(undefined);
}, "`length` parameter is undefined");

var result = new ArrayBuffer(null);
assert.sameValue(result.byteLength, 0, "`length` parameter is null");

var result = new ArrayBuffer(true);
assert.sameValue(result.byteLength, 1, "`length` parameter is a Boolean");

var result = new ArrayBuffer("");
assert.sameValue(result.byteLength, 0, "`length` parameter is a String");

assert.throws(TypeError, function() {
  new ArrayBuffer(Symbol());
}, "`length` parameter is a Symbol");
