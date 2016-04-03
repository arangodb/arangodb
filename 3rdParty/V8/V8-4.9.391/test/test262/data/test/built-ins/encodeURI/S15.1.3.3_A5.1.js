// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of encodeURI has the attribute DontEnum
es5id: 15.1.3.3_A5.1
description: Checking use propertyIsEnumerable, for-in
---*/

//CHECK#1
if (encodeURI.propertyIsEnumerable('length') !== false) {
  $ERROR('#1: encodeURI.propertyIsEnumerable(\'length\') === false. Actual: ' + (encodeURI.propertyIsEnumerable('length')));
}

//CHECK#2
var result = true;
for (var p in encodeURI){
  if (p === "length") {
    result = false;
  }  
}

if (result !== true) {
  $ERROR('#2: result = true; for (p in encodeURI) { if (p === "length") result = false; }  result === true;');
}
