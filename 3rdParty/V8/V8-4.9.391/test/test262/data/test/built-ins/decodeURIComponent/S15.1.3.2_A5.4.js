// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of decodeURIComponent is 1
es5id: 15.1.3.2_A5.4
description: decodeURIComponent.length === 1
---*/

//CHECK#1
if (decodeURIComponent.length !== 1) {
  $ERROR('#1: decodeURIComponent.length === 1. Actual: ' + (decodeURIComponent.length));
}
