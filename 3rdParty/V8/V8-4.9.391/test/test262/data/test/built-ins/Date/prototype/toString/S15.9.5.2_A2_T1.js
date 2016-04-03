// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "toString" is 0
es5id: 15.9.5.2_A2_T1
description: The "length" property of the "toString" is 0
---*/

if(Date.prototype.toString.hasOwnProperty("length") !== true){
  $ERROR('#1: The toString has a "length" property');
}

if(Date.prototype.toString.length !== 0){
  $ERROR('#2: The "length" property of the toString is 0');
}
