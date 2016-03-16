// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.NEGATIVE_INFINITY is ReadOnly
es5id: 15.7.3.5_A2
description: Checking if varying Number.NEGATIVE_INFINITY fails
includes: [propertyHelper.js]
---*/

// CHECK#1
verifyNotWritable(Number, "NEGATIVE_INFINITY", null, 1);
if (isFinite(Number.NEGATIVE_INFINITY)) {
  $ERROR('#1: Number.NEGATIVE_INFINITY = 1; Number.NEGATIVE_INFINITY === -Infinity');
} else { 
  if (Number.NEGATIVE_INFINITY >= 0) {
    $ERROR('#1: Number.NEGATIVE_INFINITY = 1; Number.NEGATIVE_INFINITY === -Infinity');
  }
}
