// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If the property doesn't have the DontDelete attribute, remove the property
es5id: 11.4.1_A3.3_T3
description: Checking declared variable
---*/

//CHECK#1
function MyFunction(){};
var MyObjectVar = new MyFunction();
MyObjectVar.prop = 1;
delete MyObjectVar.prop;
if (MyObjectVar.prop !== undefined) {
  $ERROR('#1: function MyFunction(){}; var MyObjectVar = new MyFunction(); MyFunction.prop = 1; delete MyObjectVar.prop; MyObjectVar.prop === undefined. Actual: ' + (MyObjectVar.prop));
}
