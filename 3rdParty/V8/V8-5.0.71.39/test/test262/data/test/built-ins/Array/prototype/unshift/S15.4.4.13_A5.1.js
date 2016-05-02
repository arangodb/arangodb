// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of unshift has the attribute DontEnum
es5id: 15.4.4.13_A5.1
description: Checking use propertyIsEnumerable, for-in
---*/

//CHECK#1
if (Array.prototype.unshift.propertyIsEnumerable('length') !== false) {
  $ERROR('#1: Array.prototype.unshift.propertyIsEnumerable(\'length\') === false. Actual: ' + (Array.prototype.unshift.propertyIsEnumerable('length')));
}

//CHECK#2
var result = true;
for (var p in Array.prototype.unshift){
  if (p === "length") {
    result = false;
  }  
}

if (result !== true) {
  $ERROR('#2: result = true; for (p in Array.prototype.unshift) { if (p === "length") result = false; }  result === true;');
}
