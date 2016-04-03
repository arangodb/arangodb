// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Arrays of differing lengths are not equivalent.
includes: [compareArray.js]
---*/

if (compareArray([], [undefined]) !== false) {
  $ERROR('Arrays of differing lengths are not equivalent.');
}

if (compareArray([undefined], []) !== false) {
  $ERROR('Arrays of differing lengths are not equivalent.');
}
