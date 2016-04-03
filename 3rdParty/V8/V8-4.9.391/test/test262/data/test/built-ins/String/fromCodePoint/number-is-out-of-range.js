// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.2.2
description: >
  Throw a RangeError if an argument is < 0 or > 0x10FFFF.
info: >
  String.fromCodePoint ( ...codePoints )

  1. Let codePoints be a List containing the arguments passed to this function.
  2. Let length be the number of elements in codePoints.
  3. Let elements be a new List.
  4. Let nextIndex be 0.
  5. Repeat while nextIndex < length
    a. Let next be codePoints[nextIndex].
    b. Let nextCP be ToNumber(next).
    c. ReturnIfAbrupt(nextCP).
    d. If SameValue(nextCP, ToInteger(nextCP)) is false, throw a RangeError
    exception.
    e. If nextCP < 0 or nextCP > 0x10FFFF, throw a RangeError exception.
  ...
---*/

assert.throws(RangeError, function() {
  String.fromCodePoint(-1);
});

assert.throws(RangeError, function() {
  String.fromCodePoint(1, -1);
});

assert.throws(RangeError, function() {
  String.fromCodePoint(1114112);
});

assert.throws(RangeError, function() {
  String.fromCodePoint(Infinity);
});
