// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of pop has the attribute DontEnum
es5id: 15.4.4.6_A5.1
description: Checking use propertyIsEnumerable, for-in
---*/

//CHECK#1
if (Array.prototype.pop.propertyIsEnumerable('length') !== false) {
  $ERROR('#1: Array.prototype.pop.propertyIsEnumerable(\'length\') === false. Actual: ' + (Array.prototype.pop.propertyIsEnumerable('length')));
}

//CHECK#2
var result = true;
for (var p in Array.prototype.pop){
  if (p === "length") {
    result = false;
  }  
}

if (result !== true) {
  $ERROR('#2: result = true; for (p in Array.prototype.pop) { if (p === "length") result = false; }  result === true;');
}
