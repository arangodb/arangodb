// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If thisArg is not null(defined) the called function is passed
    ToObject(thisArg) as the this value
es5id: 15.3.4.4_A5_T3
description: thisArg is string
flags: [noStrict]
---*/

var obj="soap";

var retobj = ( function(){this.touched= true; return this;} ).call(obj);

//CHECK#1
if (typeof obj.touched !== "undefined") {
  $ERROR('#1: If thisArg is not null(defined) the called function is passed ToObject(thisArg) as the this value');
}

//CHECK#2
if (!(retobj["touched"])) {
  $ERROR('#2: If thisArg is not null(defined) the called function is passed ToObject(thisArg) as the this value');
}
