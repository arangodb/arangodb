// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The RegExp.prototype ignoreCase property has the attribute DontEnum
es5id: 15.10.7.3_A8
description: >
    Checking if enumerating the ignoreCase property of
    RegExp.prototype fails
---*/

var __re = RegExp.prototype;

//CHECK#0
if (__re.hasOwnProperty('ignoreCase') !== true) {
  $ERROR('#0: __re = RegExp.prototype; __re.hasOwnProperty(\'ignoreCase\') === true');
}

 //CHECK#1
if (__re.propertyIsEnumerable('ignoreCase') !== false) {
  $ERROR('#1: __re = RegExp.prototype; __re.propertyIsEnumerable(\'ignoreCase\') === false');
}

 //CHECK#2
var count = 0
for (var p in __re){
  if (p==="ignoreCase") count++   
}

if (count !== 0) {
  $ERROR('#2: count = 0; __re = RegExp.prototype; for (p in __re){ if (p==="ignoreCase") count++; } count === 0. Actual: ' + (count));
}
