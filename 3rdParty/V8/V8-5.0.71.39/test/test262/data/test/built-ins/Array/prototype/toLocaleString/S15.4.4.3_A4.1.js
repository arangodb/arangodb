// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of toLocaleString has the attribute DontEnum
es5id: 15.4.4.3_A4.1
description: Checking use propertyIsEnumerable, for-in
---*/

//CHECK#1
if (Array.prototype.toLocaleString.propertyIsEnumerable('length') !== false) {
  $ERROR('#1: Array.prototype.toLocaleString.propertyIsEnumerable(\'length\') === false. Actual: ' + (Array.prototype.toLocaleString.propertyIsEnumerable('length')));
}

//CHECK#2
var result = true;
for (var p in Array.prototype.toLocaleString){
  if (p === "length") {
    result = false;
  }  
}

if (result !== true) {
  $ERROR('#2: result = true; for (p in Array.prototype.toLocaleString) { if (p === "length") result = false; }  result === true;');
}
