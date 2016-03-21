// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.3
description: >
  start argument is coerced to an integer value.
info: >
  22.1.3.3 Array.prototype.copyWithin (target, start [ , end ] )

  ...
  8. Let relativeStart be ToInteger(start).
  ...
includes: [compareArray.js]
---*/

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, undefined),
    [0, 0, 1, 2]
  ),
  'undefined value coerced to 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, false),
    [0, 0, 1, 2]
  ),
  'false value coerced to 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, NaN),
    [0, 0, 1, 2]
  ),
  'NaN value coerced to 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, null),
    [0, 0, 1, 2]
  ),
  'null value coerced to 0'
);


assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(0, true),
    [1, 2, 3, 3]
  ),
  'true value coerced to 1'
);


assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(0, '1'),
    [1, 2, 3, 3]
  ),
  'string "1" value coerced to 1'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1, 0.5),
    [0, 0, 1, 2]
  ),
  '0.5 float value coerced to integer 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(0, 1.5),
    [1, 2, 3, 3]
  ),
  '1.5 float value coerced to integer 1'
);