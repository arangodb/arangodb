// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The RegExp instance lastIndex property has the attribute DontEnum
es5id: 15.10.7.5_A8
description: >
    Checking if enumerating the lastIndex property of RegExp instance
    fails
---*/

var __re = new RegExp("A?B");

//CHECK#0
if (__re.hasOwnProperty('lastIndex') !== true) {
  $ERROR('#0: __re = new RegExp("A?B"); __re.hasOwnProperty(\'lastIndex\') === true');
}

 //CHECK#1
if (__re.propertyIsEnumerable('lastIndex') !== false) {
  $ERROR('#1: __re = new RegExp("A?B"); __re.propertyIsEnumerable(\'lastIndex\') === false');
}

 //CHECK#2
var count = 0
for (var p in __re){
  if (p==="lastIndex") count++   
}

if (count !== 0) {
  $ERROR('#2: count = 0; __re = new RegExp("A?B"); for (p in __re){ if (p==="lastIndex") count++; } count === 0. Actual: ' + (count));
}
