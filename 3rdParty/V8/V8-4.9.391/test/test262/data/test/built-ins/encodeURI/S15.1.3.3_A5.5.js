// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The encodeURI property has the attribute DontEnum
es5id: 15.1.3.3_A5.5
description: Checking use propertyIsEnumerable, for-in
---*/

//CHECK#1
if (this.propertyIsEnumerable('encodeURI') !== false) {
  $ERROR('#1: this.propertyIsEnumerable(\'encodeURI\') === false. Actual: ' + (this.propertyIsEnumerable('encodeURI')));
}

//CHECK#2
var result = true;
for (var p in this){
  if (p === "encodeURI") {
    result = false;
  }  
}

if (result !== true) {
  $ERROR('#2: result = true; for (p in this) { if (p === "encodeURI") result = false; }  result === true;');
}
