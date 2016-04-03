// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Value Property LOG10E of the Math Object has the attribute DontEnum
es5id: 15.8.1.5_A2
description: Checking if Math.LOG10E property has the attribute DontEnum
---*/

// CHECK#1
for(var x in Math) {
  if(x === "LOG10E") {
    $ERROR('#1: Value Property LOG10E of the Math Object hasn\'t attribute DontEnum: \'for(x in Math) {x==="LOG10E"}\'');
  }
}
