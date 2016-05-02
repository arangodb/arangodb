// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.POSITIVE_INFINITY is +Infinity
es5id: 15.7.3.6_A1
description: Checking sign and finiteness of Number.POSITIVE_INFINITY
---*/

// CHECK#1
if (isFinite(Number.POSITIVE_INFINITY) !== false) {
  $ERROR('#1: Number.POSITIVE_INFINITY === Not-a-Finite');
} else {
  if ((Number.POSITIVE_INFINITY > 0) !== true) {
    $ERROR('#1: Number.POSITIVE_INFINITY === +Infinity');
  }
}
