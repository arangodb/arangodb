// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Object.prototype.isPrototypeOf has not prototype property
es5id: 15.2.4.6_A6
description: >
    Checking if obtaining the prototype property of
    Object.prototype.isPrototypeOf fails
---*/

//CHECK#1
if (Object.prototype.isPrototypeOf.prototype !== undefined) {
  $ERROR('#1: Object.prototype.isPrototypeOf has not prototype property'+Object.prototype.isPrototypeOf.prototype);
}
//
