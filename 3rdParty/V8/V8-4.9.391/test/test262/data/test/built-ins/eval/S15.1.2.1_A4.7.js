// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The eval property can't be used as constructor
es5id: 15.1.2.1_A4.7
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new eval();
  $ERROR('#1.1: new eval() throw TypeError. Actual: ' + (new eval()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    $ERROR('#1.2: new eval() throw TypeError. Actual: ' + (e));
  }
}
