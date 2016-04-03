// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.NEGATIVE_INFINITY is -Infinity
es5id: 15.7.3.5_A1
description: Checking sign and finiteness of Number.NEGATIVE_INFINITY
---*/

// CHECK#1
if (isFinite(Number.NEGATIVE_INFINITY) !== false) {
  $ERROR('#1: Number.NEGATIVE_INFINITY === Not-a-Finite');
} else {
  if ((Number.NEGATIVE_INFINITY < 0) !== true) {
    $ERROR('#1: Number.NEGATIVE_INFINITY === -Infinity');
  }
}
