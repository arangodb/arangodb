// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The isNaN property can't be used as constructor
es5id: 15.1.2.4_A2.7
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new isNaN();
  $ERROR('#1.1: new isNaN() throw TypeError. Actual: ' + (new isNaN()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    $ERROR('#1.2: new isNaN() throw TypeError. Actual: ' + (e));
  }
}
