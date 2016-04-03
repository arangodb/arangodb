// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: RegExp instance has no [[Construct]] internal method
es5id: 15.10.7_A2_T1
description: Checking if creating new RegExp instance fails
---*/

//CHECK#1
try {
  $ERROR('#1.1: new /z/() throw TypeError. Actual: ' + (new /z/()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    $ERROR('#1.2: new /z/() throw TypeError. Actual: ' + (e));
  }
}
