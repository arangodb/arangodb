// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.2.2
description: >
  The the arguments list is empty, an empty string is returned.
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

assert.sameValue(String.fromCodePoint(), '');
