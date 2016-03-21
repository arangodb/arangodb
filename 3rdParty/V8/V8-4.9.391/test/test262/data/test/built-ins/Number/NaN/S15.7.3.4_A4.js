// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.NaN has the attribute DontEnum
es5id: 15.7.3.4_A4
description: Checking if enumerating Number.NaN fails
---*/

//CHECK#1
for(var x in Number) {
  if(x === "NaN") {
    $ERROR('#1: Number.NaN has the attribute DontEnum');
  }
}

if (Number.propertyIsEnumerable('NaN')) {
  $ERROR('#2: Number.NaN has the attribute DontEnum');
}
