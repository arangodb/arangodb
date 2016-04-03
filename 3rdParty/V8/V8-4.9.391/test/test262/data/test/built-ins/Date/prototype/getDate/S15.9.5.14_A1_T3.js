// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getDate" has { DontEnum } attributes
es5id: 15.9.5.14_A1_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getDate')) {
  $ERROR('#1: The Date.prototype.getDate property has the attribute DontEnum');
}

for(var x in Date.prototype) {
  if(x === "getDate") {
    $ERROR('#2: The Date.prototype.getDate has the attribute DontEnum');
  }
}
