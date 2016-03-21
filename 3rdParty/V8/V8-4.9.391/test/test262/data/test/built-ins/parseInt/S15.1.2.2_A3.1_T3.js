// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToNumber
es5id: 15.1.2.2_A3.1_T3
description: Checking for undefined and null
---*/

//CHECK#1
if (parseInt("11", undefined) !== parseInt("11", 10)) {
  $ERROR('#1: parseInt("11", undefined) === parseInt("11", 10). Actual: ' + (parseInt("11", undefined)));
}

//CHECK#2
if (parseInt("11", null) !== parseInt("11", 10)) {
  $ERROR('#2: parseInt("11", null) === parseInt("11", 10). Actual: ' + (parseInt("11", null)));
}
