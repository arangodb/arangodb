// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If GetBase(x) doesn't have a property GetPropertyName(x), return true
es5id: 11.4.1_A2.2_T1
description: Checking undeclared variable case
flags: [noStrict]
---*/

//CHECK#1
if (delete x !== true) {
  $ERROR('#1: delete x === true');
}
