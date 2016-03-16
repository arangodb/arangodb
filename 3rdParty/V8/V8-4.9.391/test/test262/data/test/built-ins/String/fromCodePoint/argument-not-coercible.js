// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.2.2
description: >
  Return abrupt from ToNumber(next).
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
---*/

var obj = {};
Object.defineProperty(obj, 'item', {
  get: function() {
    throw new Test262Error();
  }
});

assert.throws(Test262Error, function() {
  String.fromCodePoint({
    valueOf: function() {
      throw new Test262Error();
    }
  });
});

assert.throws(Test262Error, function() {
  String.fromCodePoint(42, {
    valueOf: function() {
      throw new Test262Error();
    }
  });
});
