// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The [[Prototype]] property of the newly constructed object
    is set to the original Array prototype object, the one that
    is the initial value of Array.prototype
es5id: 15.4.2.2_A1.1_T1
description: >
    Create new property of Array.prototype. When new Array object has
    this property
---*/

//CHECK#1
Array.prototype.myproperty = 1;
var x = new Array(0); 
if (x.myproperty !== 1) {
  $ERROR('#1: Array.prototype.myproperty = 1; var x = new Array(0); x.myproperty === 1. Actual: ' + (x.myproperty));
}
