// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.3.2-0-5
description: >
    Array.isArray return true if its argument is an Array
    (Array.prototype)
---*/

  var b = Array.isArray(Array.prototype);

assert.sameValue(b, true, 'b');
