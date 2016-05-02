// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.2.2
description: >
  Returns the String value with the code unit for the given coerced types.
info: >
  String.fromCodePoint ( ...codePoints )

  1. Let codePoints be a List containing the arguments passed to this function.
  ...
  5. Repeat while nextIndex < length
    a. Let next be codePoints[nextIndex].
    b. Let nextCP be ToNumber(next).
    ...
  6. Return the String value whose elements are, in order, the elements in the
  List elements. If length is 0, the empty string is returned.

  Ref: 7.1.3 ToNumber ( argument )
---*/

assert.sameValue(String.fromCodePoint(null), '\x00');
assert.sameValue(String.fromCodePoint(false), '\x00');
assert.sameValue(String.fromCodePoint(true), '\x01');
assert.sameValue(String.fromCodePoint('42'), '\x2A');
assert.sameValue(String.fromCodePoint('042'), '\x2A');
assert.sameValue(
  String.fromCodePoint({ valueOf: function() { return 31; } }),
  '\x1F'
);
