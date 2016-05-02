// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If one expression is undefined or null and another is not, return false
es5id: 11.9.2_A6.2_T1
description: x is null or undefined, y is not
---*/

//CHECK#1
if ((undefined != true) !== true) {
  $ERROR('#1: (undefined != true) === true');
}

//CHECK#2
if ((undefined != 0) !== true) {
  $ERROR('#2: (undefined != 0) === true');
}

//CHECK#3
if ((undefined != "undefined") !== true) {
  $ERROR('#3: (undefined != "undefined") === true');
}

//CHECK#4
if ((undefined != {}) !== true) {
  $ERROR('#4: (undefined != {}) === true');
}

//CHECK#5
if ((null != false) !== true) {
  $ERROR('#5: (null != false) === true');
}

//CHECK#6
if ((null != 0) !== true) {
  $ERROR('#6: (null != 0) === true');
}

//CHECK#7
if ((null != "null") !== true) {
  $ERROR('#7: (null != "null") === true');
}

//CHECK#8
if ((null != {}) !== true) {
  $ERROR('#8: (null != {}) === true');
}
