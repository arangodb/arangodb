// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToNumber
es5id: 15.1.2.2_A3.1_T6
description: Checking for String object
---*/

//CHECK#1
if (parseInt("11", new String("2")) !== parseInt("11", 2)) {
  $ERROR('#1: parseInt("11", new String("2")) === parseInt("11", 2). Actual: ' + (parseInt("11", new String("2"))));
}

//CHECK#2
if (parseInt("11", new String("Infinity")) !== parseInt("11", Infinity)) {
  $ERROR('#2: parseInt("11", new String("Infinity")) === parseInt("11", Infinity). Actual: ' + (parseInt("11", new String("Infinity"))));
}
