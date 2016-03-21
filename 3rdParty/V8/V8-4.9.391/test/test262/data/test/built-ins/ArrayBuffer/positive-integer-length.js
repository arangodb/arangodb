// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.1.2.1
description: >
  The `length` parameter must be a positive, numeric integral value.
info: >
  ArrayBuffer( length )

  ...
  2. Let numberLength be ToNumber(length).
  3. Let byteLength be ToLength(numberLength).
  4. ReturnIfAbrupt(byteLength).
  5. If SameValueZero(numberLength, byteLength) is false, throw a RangeError exception.
  ...
---*/

assert.throws(RangeError, function() {
  new ArrayBuffer(-10);
}, "`length` parameter is negative");

assert.throws(RangeError, function() {
  new ArrayBuffer(3.8);
}, "`length` parameter is not integral value");

assert.throws(RangeError, function() {
  // Math.pow(2, 53) = 9007199254740992
  new ArrayBuffer(9007199254740992);
}, "`length` parameter is too large");

assert.throws(RangeError, function() {
  new ArrayBuffer(+Infinity);
}, "`length` parameter is positive Infinity");

assert.throws(RangeError, function() {
  new ArrayBuffer(-Infinity);
}, "`length` parameter is negative Infinity");

assert.throws(RangeError, function() {
  new ArrayBuffer(NaN);
}, "`length` parameter is NaN");
