// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.3
description: >
  Set values with negative out of bounds end argument.
info: >
  22.1.3.3 Array.prototype.copyWithin (target, start [ , end ] )

  ...
  11. If end is undefined, let relativeEnd be len; else let relativeEnd be
  ToInteger(end).
  12. ReturnIfAbrupt(relativeEnd).
  13. If relativeEnd < 0, let final be max((len + relativeEnd),0); else let
  final be min(relativeEnd, len).
  ...
includes: [compareArray.js]
---*/

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(0, 1, -10),
    [0, 1, 2, 3]
  ),
  '[0, 1, 2, 3].copyWithin(0, 1, -10) -> [0, 1, 2, 3]'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(0, -2, -10),
    [0, 1, 2, 3]
  ),
  '[0, 1, 2, 3].copyWithin(0, -2, -10) -> [0, 1, 2, 3]'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(0, -9, -10),
    [0, 1, 2, 3]
  ),
  '[0, 1, 2, 3].copyWithin(0, -9, -10) -> [0, 1, 2, 3]'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(-3, -2, -10),
    [0, 1, 2, 3]
  ),
  '[0, 1, 2, 3].copyWithin(-3, -2, -10) -> [0, 1, 2, 3]'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(-7, -8, -9),
    [0, 1, 2, 3]
  ),
  '[0, 1, 2, 3].copyWithin(-7, -8, -9) -> [0, 1, 2, 3]'
);