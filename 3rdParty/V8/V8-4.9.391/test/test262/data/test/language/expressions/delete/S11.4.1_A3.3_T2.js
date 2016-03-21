// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If the property doesn't have the DontDelete attribute, remove the property
es5id: 11.4.1_A3.3_T2
description: Checking declared variable
---*/

//CHECK#1
function MyFunction(){};
MyFunction.prop = 1;
delete MyFunction.prop;
if (MyFunction.prop !== undefined) {
  $ERROR('#1: function MyFunction(){}; MyFunction.prop = 1; delete MyFunction.prop; MyFunction.prop === undefined. Actual: ' + (MyFunction.prop));

}
