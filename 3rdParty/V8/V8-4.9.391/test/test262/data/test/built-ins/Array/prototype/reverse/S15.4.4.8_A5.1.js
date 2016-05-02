// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of reverse has the attribute DontEnum
es5id: 15.4.4.8_A5.1
description: Checking use propertyIsEnumerable, for-in
---*/

//CHECK#1
if (Array.prototype.reverse.propertyIsEnumerable('length') !== false) {
  $ERROR('#1: Array.prototype.reverse.propertyIsEnumerable(\'length\') === false. Actual: ' + (Array.prototype.reverse.propertyIsEnumerable('length')));
}

//CHECK#2
var result = true;
for (p in Array.prototype.reverse){
  if (p === "length") {
    var result = false;
  }  
}

if (result !== true) {
  $ERROR('#2: result = true; for (p in Array.prototype.reverse) { if (p === "length") result = false; }  result === true;');
}
