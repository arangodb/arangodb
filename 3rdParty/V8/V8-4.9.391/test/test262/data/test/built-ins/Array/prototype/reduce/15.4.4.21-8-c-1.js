// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-8-c-1
description: >
    Array.prototype.reduce throws TypeError when Array is empty and
    initialValue is not present
---*/

  function callbackfn(prevVal, curVal, idx, obj)
  {
  }

  var arr = new Array(10);
assert.throws(TypeError, function() {
    arr.reduce(callbackfn);
});
