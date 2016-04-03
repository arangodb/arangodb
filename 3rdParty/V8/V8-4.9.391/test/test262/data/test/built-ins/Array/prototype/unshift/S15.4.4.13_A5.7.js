// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The unshift property of Array can't be used as constructor
es5id: 15.4.4.13_A5.7
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new Array.prototype.unshift();
  $ERROR('#1.1: new Array.prototype.unshift() throw TypeError. Actual: ' + (new Array.prototype.unshift()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    $ERROR('#1.2: new Array.prototype.unshift() throw TypeError. Actual: ' + (e));
  }
}
