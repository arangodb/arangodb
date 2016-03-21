// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of parseFloat is 1
es5id: 15.1.2.3_A7.4
description: parseFloat.length === 1
---*/

//CHECK#1
if (parseFloat.length !== 1) {
  $ERROR('#1: parseFloat.length === 1. Actual: ' + (parseFloat.length));
}
