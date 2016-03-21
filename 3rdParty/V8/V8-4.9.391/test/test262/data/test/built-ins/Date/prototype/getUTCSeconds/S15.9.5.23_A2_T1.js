// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCSeconds" is 0
es5id: 15.9.5.23_A2_T1
description: The "length" property of the "getUTCSeconds" is 0
---*/

if(Date.prototype.getUTCSeconds.hasOwnProperty("length") !== true){
  $ERROR('#1: The getUTCSeconds has a "length" property');
}

if(Date.prototype.getUTCSeconds.length !== 0){
  $ERROR('#2: The "length" property of the getUTCSeconds is 0');
}
