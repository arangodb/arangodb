// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Function prototype object is itself a Function object (its [[Class]]
    is "Function")
es5id: 15.3.4_A1
description: Object.prototype.toString returns [object+[[Class]]+]
---*/

if (Object.prototype.toString.call(Function.prototype) !== "[object Function]") {
  $ERROR('#2: The Function prototype object is itself a Function ' +
         'object (its [[Class]] is "Function") (15.3.4)');
}
