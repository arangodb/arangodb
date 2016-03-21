// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    When the Object constructor is called with one argument value and
    the value is a native ECMAScript object, do not create a new object but simply return value
es5id: 15.2.2.1_A2_T5
description: The value is a Date object
---*/

var obj = new Date(1978,3);

var n_obj = new Object(obj);

//CHECK#1
if (n_obj !== obj) {
  $ERROR('#1: When the Object constructor is called and if the value is an Object simply value returns.');
}

//CHECK#2
if ((n_obj.getFullYear() !== 1978)||(n_obj.getMonth() !== 3)) {
  $ERROR('#2: When the Object constructor is called and if the value is an Object simply value returns.');
}
