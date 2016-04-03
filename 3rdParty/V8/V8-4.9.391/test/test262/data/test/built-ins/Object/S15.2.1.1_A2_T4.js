// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    When the Object function is called with one argument value,
    and the value neither is null nor undefined, and is supplied, return ToObject(value)
es5id: 15.2.1.1_A2_T4
description: Calling Object function with object argument value
---*/

var obj = {flag:true};

//CHECK#1
if (typeof(obj) !== 'object') {
  $ERROR('#1: obj = {flag:true} should be an Object');
}

var n_obj = Object(obj);

//CHECK#2
if ((n_obj !== obj)||(!(n_obj['flag']))) {
  $ERROR('#2: Object({flag:true}) returns ToObject({flag:true})');
}
