// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of the toFixed method is 1
es5id: 15.7.4.5_A2_T01
description: Checking Number prototype itself
---*/

//CHECK#1
if(Number.prototype.toFixed.hasOwnProperty("length") !== true){
  $ERROR('#1: The length property of the toFixed method is 1');
}

//CHECK#2
if(Number.prototype.toFixed.length !== 1){
  $ERROR('#2: The length property of the toFixed method is 1');
}
