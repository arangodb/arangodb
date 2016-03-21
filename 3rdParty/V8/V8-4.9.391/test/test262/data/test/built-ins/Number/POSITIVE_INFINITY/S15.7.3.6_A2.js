// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.POSITIVE_INFINITY is ReadOnly
es5id: 15.7.3.6_A2
description: Checking if varying Number.POSITIVE_INFINITY fails
includes: [propertyHelper.js]
---*/

// CHECK#1
verifyNotWritable(Number, "POSITIVE_INFINITY", null, 1);
if (isFinite(Number.POSITIVE_INFINITY)) {
  $ERROR('#1: Number.POSITIVE_INFINITY = 1; Number.POSITIVE_INFINITY === +Infinity');
} else { 
  if (Number.POSITIVE_INFINITY <= 0) {
    $ERROR('#1: Number.POSITIVE_INFINITY = 1; Number.POSITIVE_INFINITY === +Infinity');
  }
}
