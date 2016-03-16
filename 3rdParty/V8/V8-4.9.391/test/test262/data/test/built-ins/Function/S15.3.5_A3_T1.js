// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: every function instance has a [[Construct]] property
es5id: 15.3.5_A3_T1
description: As constructor use Function("var x =1; this.y=2;return \"OK\";")
---*/

var FACTORY = Function("var x =1; this.y=2;return \"OK\";");
var obj = new FACTORY;

//CHECK#1
if (typeof obj !== "object") {
  $ERROR('#1: every function instance has a [[Construct]] property');
}

//CHECK#2
if (obj.constructor !== FACTORY) {
  $ERROR('#2: every function instance has a [[Construct]] property');
}

//CHECK#3
if (obj.y !== 2) {
  $ERROR('#3: every function instance has a [[Construct]] property');
}
