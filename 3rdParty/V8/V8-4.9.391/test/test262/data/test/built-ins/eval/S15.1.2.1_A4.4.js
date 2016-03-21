// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of eval is 1
es5id: 15.1.2.1_A4.4
description: eval.length === 1
---*/

//CHECK#1
if (eval.length !== 1) {
  $ERROR('#1: eval.length === 1. Actual: ' + (eval.length));
}
