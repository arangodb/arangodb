// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The length property of the newly constructed object;
    is set to the number of arguments
es5id: 15.4.2.1_A2.1_T1
description: Array constructor is given no arguments or at least two arguments
---*/

//CHECK#1
if (new Array().length !== 0) {
  $ERROR('#1: new Array().length === 0. Actual: ' + (new Array().length));
}

//CHECK#2
if (new Array(0,1,0,1).length !== 4) {
  $ERROR('#2: new Array(0,1,0,1).length === 4. Actual: ' + (new Array(0,1,0,1).length));
}

//CHECK#3
if (new Array(undefined, undefined).length !== 2) {
  $ERROR('#3: new Array(undefined, undefined).length === 2. Actual: ' + (new Array(undefined, undefined).length));
}
