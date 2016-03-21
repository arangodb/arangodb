// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype has the property "getUTCFullYear"
es5id: 15.9.5_A11_T1
description: The Date.prototype has the property "getUTCFullYear"
---*/

if(Date.prototype.hasOwnProperty("getUTCFullYear") !== true){
  $ERROR('#1: The Date.prototype has the property "getUTCFullYear"');
}
