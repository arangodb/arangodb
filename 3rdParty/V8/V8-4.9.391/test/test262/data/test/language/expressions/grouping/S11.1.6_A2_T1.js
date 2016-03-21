// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    "This" operator doesn't use GetValue. The operators "delete" and "typeof"
    can be applied to parenthesised expressions
es5id: 11.1.6_A2_T1
description: >
    Applying "delete" and "typeof" operators to an undefined variable
    and a property of an object
---*/

//CHECK#1
if (typeof (x) !== "undefined") {
  $ERROR('#1: typeof (x) === "undefined". Actual: ' + (typeof (x)));
}

var object = {};
//CHECK#2
if (delete (object.prop) !== true) {
  $ERROR('#2: var object = {}; delete (object.prop) === true');
}

//CHECK#3
if (typeof (object.prop) !== "undefined") {
  $ERROR('#3: var object = {}; typeof (object.prop) === "undefined". Actual: ' + (typeof (object.prop)));
}
