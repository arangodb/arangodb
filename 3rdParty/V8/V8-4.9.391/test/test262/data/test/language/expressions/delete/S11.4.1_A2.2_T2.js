// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If GetBase(x) doesn't have a property GetPropertyName(x), return true
es5id: 11.4.1_A2.2_T2
description: Checking Object object and Function object cases
---*/

//CHECK#1
function MyFunction(){}
var MyObject = new MyFunction();
if (delete MyObject.prop !== true) {
  $ERROR('#1: function MyFunction(){}; var MyObject = new MyFunction(); delete MyObject.prop === true');
}

//CHECK#2
var MyObject = new Object();
if (delete MyObject.prop !== true) {
  $ERROR('#2: var MyObject = new Object(); delete MyObject.prop === true');
}
