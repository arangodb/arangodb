// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-4-7
description: >
    Array.prototype.some throws TypeError if callbackfn is Object
    without a Call internal method
---*/

  var arr = new Array(10);
assert.throws(TypeError, function() {
    arr.some(new Object());
});
