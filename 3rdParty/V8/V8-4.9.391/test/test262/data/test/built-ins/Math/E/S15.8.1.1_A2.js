// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Value Property E of the Math Object has the attribute DontEnum
es5id: 15.8.1.1_A2
description: Checking if Math.E property has the attribute DontEnum
---*/

// CHECK#1
for(var x in Math) {
  if(x === "E") {
    $ERROR('#1: Value Property E of the Math Object hasn\'t attribute DontEnum: \'for(x in Math) {x==="E"}\'');
  }
}
