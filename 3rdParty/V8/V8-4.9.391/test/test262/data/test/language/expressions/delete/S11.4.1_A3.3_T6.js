// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If the property doesn't have the DontDelete attribute, remove the property
es5id: 11.4.1_A3.3_T6
description: Checking declared variable
flags: [noStrict]
---*/

//CHECK#1
function MyFunction(){};
var MyObjectVar = new MyFunction();
if (delete MyObjectNotVar !== true) {
  $ERROR('#1: function MyFunction(){}; var MyObjectNotVar = new MyFunction(); delete MyObjectNotVar === true');
}
