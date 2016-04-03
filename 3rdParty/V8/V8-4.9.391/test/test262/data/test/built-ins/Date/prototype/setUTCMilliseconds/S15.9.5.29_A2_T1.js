// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCMilliseconds" is 1
es5id: 15.9.5.29_A2_T1
description: The "length" property of the "setUTCMilliseconds" is 1
---*/

if(Date.prototype.setUTCMilliseconds.hasOwnProperty("length") !== true){
  $ERROR('#1: The setUTCMilliseconds has a "length" property');
}

if(Date.prototype.setUTCMilliseconds.length !== 1){
  $ERROR('#2: The "length" property of the setUTCMilliseconds is 1');
}
