// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype has the property "setMilliseconds"
es5id: 15.9.5_A28_T1
description: The Date.prototype has the property "setMilliseconds"
---*/

if(Date.prototype.hasOwnProperty("setMilliseconds") !== true){
  $ERROR('#1: The Date.prototype has the property "setMilliseconds"');
}
