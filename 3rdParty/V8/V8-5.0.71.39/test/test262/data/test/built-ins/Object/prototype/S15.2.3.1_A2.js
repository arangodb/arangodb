// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Object.prototype property has the attribute DontEnum
es5id: 15.2.3.1_A2
description: Checking if enumerating "Object.prototype" property fails
---*/

// CHECK#1
if (Object.propertyIsEnumerable('prototype')) {
  $ERROR('#1: the Object.prototype property has the attributes DontEnum');
}

// CHECK#2
var cout=0;

for (var p in Object){
  if (p==="prototype") cout++;
}

if (cout !== 0) {
  $ERROR('#2: the Object.prototype property has the attributes DontEnum');
}
