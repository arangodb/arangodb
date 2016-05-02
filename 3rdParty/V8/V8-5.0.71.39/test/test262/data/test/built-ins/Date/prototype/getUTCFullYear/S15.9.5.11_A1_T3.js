// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCFullYear" has { DontEnum } attributes
es5id: 15.9.5.11_A1_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getUTCFullYear')) {
  $ERROR('#1: The Date.prototype.getUTCFullYear property has the attribute DontEnum');
}

for(var x in Date.prototype) {
  if(x === "getUTCFullYear") {
    $ERROR('#2: The Date.prototype.getUTCFullYear has the attribute DontEnum');
  }
}
