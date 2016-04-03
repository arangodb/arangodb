// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If thisArg is not null(defined) the called function is passed
    ToObject(thisArg) as the this value
es5id: 15.3.4.3_A5_T1
description: thisArg is number
---*/

var obj=1;

var retobj = Function("this.touched= true; return this;").apply(obj);

//CHECK#1
if (typeof obj.touched !== "undefined") {
  $ERROR('#1: If thisArg is not null(defined) the called function is passed ToObject(thisArg) as the this value');
}

//CHECK#2
if (!(retobj["touched"])) {
  $ERROR('#2: If thisArg is not null(defined) the called function is passed ToObject(thisArg) as the this value');
}
