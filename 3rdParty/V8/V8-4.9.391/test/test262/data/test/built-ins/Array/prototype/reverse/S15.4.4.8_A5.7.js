// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The reverse property of Array can't be used as constructor
es5id: 15.4.4.8_A5.7
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new Array.prototype.reverse();
  $ERROR('#1.1: new Array.prototype.reverse() throw TypeError. Actual: ' + (new Array.prototype.reverse()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    $ERROR('#1.2: new Array.prototype.reverse() throw TypeError. Actual: ' + (e));
  }
}
