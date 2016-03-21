// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    When the Object constructor is called with one argument value and
    the type of value is String, return ToObject(string)
es5id: 15.2.2.1_A3_T3
description: Argument value is sum of empty string and number
---*/

var n_obj = new Object(""+1);

//CHECK#2
if (n_obj.constructor !== String) {
  $ERROR('#2: When the Object constructor is called with String argument return ToObject(string)');
}

//CHECK#3
if (typeof n_obj !== 'object') {
  $ERROR('#3: When the Object constructor is called with String argument return ToObject(string)');
}

//CHECK#4
if ( n_obj != "1") {
  $ERROR('#4: When the Object constructor is called with String argument return ToObject(string)');
}

//CHECK#5
if ( n_obj === "1") {
  $ERROR('#5: When the Object constructor is called with String argument return ToObject(string)');
}
