// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Array prototype object does not have a valueOf property of
    its own; however, it inherits the valueOf property from the valueOf
    property from the Object prototype Object
es5id: 15.4.4_A2.1_T2
description: >
    Change valueOf property of Object.prototype. When
    Array.prototype.valueOf also change
---*/

Object.prototype.valueOf = 1;

//CHECK#1
if (Array.prototype.valueOf !== 1) {
  $ERROR('#1: Object.prototype.valueOf = 1; Array.prototype.valueOf === 1. Actual: ' + (Array.prototype.valueOf));
}

//CHECK#2
var x = new Array();
if (x.valueOf !== 1) {
  $ERROR('#1: Object.prototype.valueOf = 1; x = new Array(); x.valueOf === 1. Actual: ' + (x.valueOf));
}
