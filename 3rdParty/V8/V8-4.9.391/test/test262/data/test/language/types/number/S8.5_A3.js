// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: NaN expression has a type Number
es5id: 8.5_A3
description: Check type of NaN
---*/

var x=NaN;

///////////////////////////////////////////////////////
// CHECK#1
if (typeof(x) !== "number"){
  $ERROR('#1: var x=NaN; typeof(x) === "number". Actual: ' + (typeof(x)));
}
//
//////////////////////////////////////////////////////////

///////////////////////////////////////////////////////
// CHECK#2
if (typeof(NaN) !== "number"){
  $ERROR('#2: typeof(NaN) === "number". Actual: ' + (typeof(NaN)));
}
//
//////////////////////////////////////////////////////////
