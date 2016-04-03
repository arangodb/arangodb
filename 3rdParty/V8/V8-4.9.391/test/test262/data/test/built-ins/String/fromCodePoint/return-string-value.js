// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.2.2
description: >
  Returns the String value whose elements are, in order, the code unit for the
  numbers in the arguments list.
info: >
  String.fromCodePoint ( ...codePoints )

  1. Let codePoints be a List containing the arguments passed to this function.
  ...
  5. Repeat while nextIndex < length
    ...
    f. Append the elements of the UTF16Encoding (10.1.1) of nextCP to the end of
    elements.
    g. Let nextIndex be nextIndex + 1.
  6. Return the String value whose elements are, in order, the elements in the
  List elements. If length is 0, the empty string is returned.
---*/

assert.sameValue(String.fromCodePoint(0), '\x00');
assert.sameValue(String.fromCodePoint(42), '*');
assert.sameValue(String.fromCodePoint(65, 90), 'AZ');
assert.sameValue(String.fromCodePoint(0x404), '\u0404');
assert.sameValue(String.fromCodePoint(0x2F804), '\uD87E\uDC04');
assert.sameValue(String.fromCodePoint(194564), '\uD87E\uDC04');
assert.sameValue(
  String.fromCodePoint(0x1D306, 0x61, 0x1D307),
  '\uD834\uDF06a\uD834\uDF07'
);
assert.sameValue(String.fromCodePoint(1114111), '\uDBFF\uDFFF');
