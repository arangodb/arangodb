// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Number property "prototype" has { DontEnum, DontDelete, ReadOnly }
    attributes
es5id: 15.7.3.1_A1_T3
description: Checking if enumerating the Number.prototype property fails
---*/

if (Number.propertyIsEnumerable('prototype')) {
  $ERROR('#1: The Number.prototype property has the attribute DontEnum');
}

for(var x in Number) {
  if(x === "prototype") {
    $ERROR('#2: The Number.prototype has the attribute DontEnum');
  }
}
