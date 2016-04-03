// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of decodeURI is 1
es5id: 15.1.3.1_A5.4
description: decodeURI.length === 1
---*/

//CHECK#1
if (decodeURI.length !== 1) {
  $ERROR('#1: decodeURI.length === 1. Actual: ' + (decodeURI.length));
}
