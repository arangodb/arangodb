// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number constructor has length property whose value is 1
es5id: 15.7.3_A8
description: Checking Number.length property
---*/

//CHECK#1
if (!Number.hasOwnProperty("length")){
  $ERROR('#1: Number constructor has length property');
}

//CHECK#2
if (Number.length !== 1) {
  $ERROR('#2: Number constructor length property value is 1');
}
