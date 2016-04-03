// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Date.prototype.constructor property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.1_A3_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.constructor.propertyIsEnumerable('length')) {
  $ERROR('#1: The Date.prototype.constructor.length property has the attribute DontEnum');
}

for(var x in Date.prototype.constructor) {
  if(x === "length") {
    $ERROR('#2: The Date.prototype.constructor.length has the attribute DontEnum');
  }
}
