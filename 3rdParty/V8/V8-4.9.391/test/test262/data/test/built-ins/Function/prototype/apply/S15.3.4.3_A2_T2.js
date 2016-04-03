// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of the apply method is 2
es5id: 15.3.4.3_A2_T2
description: Checking f.apply.length, where f is new Function
---*/

var f=new Function;

//CHECK#1
if (typeof f.apply !== "function") {
  $ERROR('#1: apply method accessed');
}

//CHECK#2
if (typeof f.apply.length === "undefined") {
  $ERROR('#2: length property of apply method defined');
}

//CHECK#3
if (f.apply.length !== 2) {
  $ERROR('#3: The length property of the apply method is 2');
}
