// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getMilliseconds" has { DontEnum } attributes
es5id: 15.9.5.24_A1_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getMilliseconds')) {
  $ERROR('#1: The Date.prototype.getMilliseconds property has the attribute DontEnum');
}

for(var x in Date.prototype) {
  if(x === "getMilliseconds") {
    $ERROR('#2: The Date.prototype.getMilliseconds has the attribute DontEnum');
  }
}
