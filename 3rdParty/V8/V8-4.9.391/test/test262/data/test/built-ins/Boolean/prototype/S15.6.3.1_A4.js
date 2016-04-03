// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Boolean.prototype has the attribute DontEnum
es5id: 15.6.3.1_A4
description: Checking if enumerating the Boolean.prototype property fails
---*/

//CHECK#1
for(x in Boolean) {
  if(x === "prototype") {
    $ERROR('#1: Boolean.prototype has the attribute DontEnum');
  }
}

if (Boolean.propertyIsEnumerable('prototype')) {
  $ERROR('#2: Boolean.prototype has the attribute DontEnum');
}
