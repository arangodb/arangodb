// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property value is 1
es5id: 15.11.3_A2_T1
description: Checking length property
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
var err1=Error("err");
if(err1.constructor.length!==1){
  $ERROR('#1: var err1=Error("err"); err1.constructor.length===1. Actual: '+err1.constructor.length);
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if(Error.constructor.length!==1){
  $ERROR('#2: Error.constructor.length===1. Actual: '+Error.constructor.length);
}
//
//////////////////////////////////////////////////////////////////////////////
