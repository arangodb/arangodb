// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If a property is added whose name is an array index,
    the length property is changed
es5id: 15.4.5.2_A2_T1
description: Checking length property
---*/

//CHECK#1
var x = [];
if (x.length !== 0) {  
  $ERROR('#1: x = []; x.length === 0. Actual: ' + (x.length));    
}

//CHECK#2
x[0] = 1;
if (x.length !== 1) {      
  $ERROR('#2: x = []; x[1] = 1; x.length === 1. Actual: ' + (x.length));
}

//CHECK#3
x[1] = 1;
if (x.length !== 2) {      
  $ERROR('#3: x = []; x[0] = 1; x[1] = 1; x.length === 2. Actual: ' + (x.length));
}

//CHECK#4
x[9] = 1;
if (x.length !== 10) {      
  $ERROR('#4: x = []; x[0] = 1; x[1] = 1; x[9] = 1; x.length === 10. Actual: ' + (x.length));
}
