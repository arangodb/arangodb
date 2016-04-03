// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The join property of Array can't be used as constructor
es5id: 15.4.4.5_A6.7
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new Array.prototype.join();
  $ERROR('#1.1: new Array.prototype.join() throw TypeError. Actual: ' + (new Array.prototype.join()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    $ERROR('#1.2: new Array.prototype.join() throw TypeError. Actual: ' + (e));
  }
}
