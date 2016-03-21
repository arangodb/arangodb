// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of push has the attribute DontEnum
es5id: 15.4.4.7_A6.1
description: Checking use propertyIsEnumerable, for-in
---*/

//CHECK#1
if (Array.prototype.push.propertyIsEnumerable('length') !== false) {
  $ERROR('#1: Array.prototype.push.propertyIsEnumerable(\'length\') === false. Actual: ' + (Array.prototype.push.propertyIsEnumerable('length')));
}

//CHECK#2
var result = true;
for (var p in Array.prototype.push){
  if (p === "length") {
    result = false;
  }  
}

if (result !== true) {
  $ERROR('#2: result = true; for (p in Array.prototype.push) { if (p === "length") result = false; }  result === true;');
}
