// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If ToBoolean(x) is false, return x
es5id: 11.11.1_A3_T1
description: >
    Type(x) is primitive boolean and Type(y) is changed between
    primitive boolean and Boolean object
---*/

//CHECK#1
if ((false && true) !== false) {
  $ERROR('#1: (false && true) === false');
}

//CHECK#2
if ((false && false) !== false) {
  $ERROR('#2: (false && false) === false');
}

//CHECK#3
if ((false && new Boolean(true)) !== false) {
  $ERROR('#3: (false && new Boolean(true)) === false');
}

//CHECK#4
if ((false && new Boolean(false)) !== false) {
  $ERROR('#4: (false && new Boolean(false)) === false');
}
