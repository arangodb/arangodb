// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Object.prototype.hasOwnProperty has not prototype property
es5id: 15.2.4.5_A6
description: >
    Checking if obtaining the prototype property of
    Object.prototype.hasOwnProperty fails
---*/

//CHECK#1
if (Object.prototype.hasOwnProperty.prototype !== undefined) {
  $ERROR('#1: Object.prototype.hasOwnProperty has not prototype property'+Object.prototype.hasOwnProperty.prototype);
}
//
