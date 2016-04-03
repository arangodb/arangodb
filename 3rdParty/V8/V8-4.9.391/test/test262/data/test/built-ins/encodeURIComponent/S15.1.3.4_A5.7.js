// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The encodeURIComponent property can't be used as constructor
es5id: 15.1.3.4_A5.7
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new encodeURIComponent();
  $ERROR('#1.1: new encodeURIComponent() throw TypeError. Actual: ' + (new encodeURIComponent()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    $ERROR('#1.2: new encodeURIComponent() throw TypeError. Actual: ' + (e));
  }
}
