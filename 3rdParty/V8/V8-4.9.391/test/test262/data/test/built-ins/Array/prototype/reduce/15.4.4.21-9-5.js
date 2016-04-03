// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-9-5
description: >
    Array.prototype.reduce - callbackfn not called for array with one
    element
---*/

  var callCnt = 0;
  function callbackfn(prevVal, curVal, idx, obj)
  {
    callCnt++;
    return 2;
  }

  var arr = [1];

assert.sameValue(arr.reduce(callbackfn), 1, 'arr.reduce(callbackfn)');
assert.sameValue(callCnt, 0, 'callCnt');
