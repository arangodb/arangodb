// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toLocaleString" has { DontEnum } attributes
es5id: 15.9.5.5_A1_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('toLocaleString')) {
  $ERROR('#1: The Date.prototype.toLocaleString property has the attribute DontEnum');
}

for(var x in Date.prototype) {
  if(x === "toLocaleString") {
    $ERROR('#2: The Date.prototype.toLocaleString has the attribute DontEnum');
  }
}
