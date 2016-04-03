// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.setTime property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
es5id: 15.9.5.27_A3_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.setTime.propertyIsEnumerable('length')) {
  $ERROR('#1: The Date.prototype.setTime.length property has the attribute DontEnum');
}

for(var x in Date.prototype.setTime) {
  if(x === "length") {
    $ERROR('#2: The Date.prototype.setTime.length has the attribute DontEnum');
  }
}
