// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of encodeURIComponent is 1
es5id: 15.1.3.4_A5.4
description: encodeURIComponent.length === 1
---*/

//CHECK#1
if (encodeURIComponent.length !== 1) {
  $ERROR('#1: encodeURIComponent.length === 1. Actual: ' + (encodeURIComponent.length));
}
