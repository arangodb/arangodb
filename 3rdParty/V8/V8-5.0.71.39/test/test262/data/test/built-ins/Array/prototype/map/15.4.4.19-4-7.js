// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-4-7
description: >
    Array.prototype.map throws TypeError if callbackfn is Object
    without Call internal method
---*/

  var arr = new Array(10);
assert.throws(TypeError, function() {
    arr.map(new Object());
});
