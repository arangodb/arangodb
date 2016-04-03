// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    pattern is an object R whose [[Class]] property is "RegExp" and flags
    is not undefined. If ToString(pattern) is not a valid flags arguments,
    then throw a SyntaxError exception
es5id: 15.10.4.1_A2_T2
description: >
    Checking if execution of "new RegExp(pattern, {})", where the
    pattern is "/1?1/mig", fails
---*/

//CHECK#1
try {
  $ERROR('#1.1: new RegExp(/1?1/mig, {}) throw SyntaxError. Actual: ' + (new RegExp(/1?1/mig, {})));
} catch (e) {
  if ((e instanceof SyntaxError) !== true) {
    $ERROR('#1.2: new RegExp(/1?1/mig, {}) throw SyntaxError. Actual: ' + (e));
  }
}
