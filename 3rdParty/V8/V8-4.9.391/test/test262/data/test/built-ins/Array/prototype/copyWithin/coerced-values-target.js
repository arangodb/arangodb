// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.3
description: >
  target argument is coerced to an integer value.
info: >
  22.1.3.3 Array.prototype.copyWithin (target, start [ , end ] )

  ...
  5. Let relativeTarget be ToInteger(target).
  ...
includes: [compareArray.js]
---*/

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(undefined, 1),
    [1, 2, 3, 3]
  ),
  'undefined value coerced to 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(false, 1),
    [1, 2, 3, 3]
  ),
  'false value coerced to 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(NaN, 1),
    [1, 2, 3, 3]
  ),
  'NaN value coerced to 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(null, 1),
    [1, 2, 3, 3]
  ),
  'null value coerced to 0'
);


assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(true, 0),
    [0, 0, 1, 2]
  ),
  'true value coerced to 1'
);


assert(
  compareArray(
    [0, 1, 2, 3].copyWithin('1', 0),
    [0, 0, 1, 2]
  ),
  'string "1" value coerced to 1'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(0.5, 1),
    [1, 2, 3, 3]
  ),
  '0.5 float value coerced to integer 0'
);

assert(
  compareArray(
    [0, 1, 2, 3].copyWithin(1.5, 0),
    [0, 0, 1, 2]
  ),
  '1.5 float value coerced to integer 1'
);