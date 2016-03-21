// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCDate" is 1
es5id: 15.9.5.37_A2_T1
description: The "length" property of the "setUTCDate" is 1
---*/

if(Date.prototype.setUTCDate.hasOwnProperty("length") !== true){
  $ERROR('#1: The setUTCDate has a "length" property');
}

if(Date.prototype.setUTCDate.length !== 1){
  $ERROR('#2: The "length" property of the setUTCDate is 1');
}
