// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: First expression is evaluated first, and then second expression
es5id: 11.9.5_A2.4_T4
description: Checking undeclarated variables
flags: [noStrict]
---*/

//CHECK#1
if ((y = 1) !== y) {
  $ERROR('#1: (y = 1) === y');
}
