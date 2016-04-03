// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getMilliseconds" has { DontEnum } attributes
es5id: 15.9.5.24_A1_T2
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getMilliseconds  === false) {
  $ERROR('#1: The Date.prototype.getMilliseconds property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getMilliseconds')) {
  $ERROR('#2: The Date.prototype.getMilliseconds property has not the attributes DontDelete');
}
