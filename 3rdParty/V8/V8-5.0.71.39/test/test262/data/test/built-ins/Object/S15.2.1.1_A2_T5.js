// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    When the Object function is called with one argument value,
    and the value neither is null nor undefined, and is supplied, return ToObject(value)
es5id: 15.2.1.1_A2_T5
description: Calling Object function with NaN argument value
---*/

var num = NaN;

// CHECK#1
if(typeof num  !== 'number'){
  $ERROR('#1: num = NaN should have number type');
}

var obj = Object(num);

//CHECK#2
if (obj.constructor !== Number) {
  $ERROR('#2: Object(NaN) returns ToObject(NaN)');
}

//CHECK#3
if (typeof obj!=="object") {
  $ERROR('#2: Object(NaN) returns ToObject(NaN)');
}
//
