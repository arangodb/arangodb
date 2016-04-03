// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "toLocaleDateString" is 0
es5id: 15.9.5.6_A2_T1
description: The "length" property of the "toLocaleDateString" is 0
---*/

if(Date.prototype.toLocaleDateString.hasOwnProperty("length") !== true){
  $ERROR('#1: The toLocaleDateString has a "length" property');
}

if(Date.prototype.toLocaleDateString.length !== 0){
  $ERROR('#2: The "length" property of the toLocaleDateString is 0');
}
