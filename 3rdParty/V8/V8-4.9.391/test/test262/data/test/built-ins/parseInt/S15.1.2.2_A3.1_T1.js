// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToNumber
es5id: 15.1.2.2_A3.1_T1
description: Checking for boolean primitive
---*/

//CHECK#1
if (parseInt("11", false) !== parseInt("11", 10)) {
  $ERROR('#1: parseInt("11", false) === parseInt("11", 10). Actual: ' + (parseInt("11", false)));
}

//CHECK#2
if (isNaN(parseInt("11", true)) !== true) {
  $ERROR('#2: parseInt("11", true) === Not-a-Number. Actual: ' + (parseInt("11", true)));
}
