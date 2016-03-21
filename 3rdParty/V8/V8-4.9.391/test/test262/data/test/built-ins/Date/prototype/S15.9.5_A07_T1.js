// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype has the property "toLocaleTimeString"
es5id: 15.9.5_A07_T1
description: The Date.prototype has the property "toLocaleTimeString"
---*/

if(Date.prototype.hasOwnProperty("toLocaleTimeString") !== true){
  $ERROR('#1: The Date.prototype has the property "toLocaleTimeString"');
}
