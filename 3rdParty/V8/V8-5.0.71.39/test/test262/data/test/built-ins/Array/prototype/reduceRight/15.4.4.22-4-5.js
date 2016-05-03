// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-4-5
description: >
    Array.prototype.reduceRight throws TypeError if callbackfn is
    number
---*/

  var arr = new Array(10);
assert.throws(TypeError, function() {
    arr.reduceRight(5);
});
