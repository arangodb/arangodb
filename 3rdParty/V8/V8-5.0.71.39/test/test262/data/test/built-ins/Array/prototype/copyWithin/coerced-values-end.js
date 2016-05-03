// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.3
description: >
  end argument is coerced to an integer values.
info: >
  22.1.3.3 Array.prototype.copyWithin (target, start [ , end ] )

  ...
  11. If end is undefined, let relativeEnd be len; else let relativeEnd be
  ToInteger(end).
  ...
includes: [compareArray.js]
---*/

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, 0, null),
    [0, 1, 2, 3]
  ),
  'null value coerced to 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, 0, NaN),
    [0, 1, 2, 3]
  ),
  'NaN value coerced to 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, 0, false),
    [0, 1, 2, 3]
  ),
  'false value coerced to 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, 0, true),
    [0, 0, 2, 3]
  ),
  'true value coerced to 1'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, 0, '-2'),
    [0, 0, 1, 3]
  ),
  'string "-2" value coerced to integer -2'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, 0, -2.5),
    [0, 0, 1, 3]
  ),
  'float -2.5 value coerced to integer -2'
);
