// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Function.prototype property has the attribute DontEnum
es5id: 15.3.3.1_A2
description: Checking if enumerating the Function.prototype property fails
---*/

// CHECK#1
if (Function.propertyIsEnumerable('prototype')) {
  $ERROR('#1: the Function.prototype property has the attributes DontEnum');
}

// CHECK#2
var count=0;

for (var p in Function){
  if (p==="prototype") count++;
}

if (count !== 0) {
  $ERROR('#2: the Function.prototype property has the attributes DontEnum');
}
