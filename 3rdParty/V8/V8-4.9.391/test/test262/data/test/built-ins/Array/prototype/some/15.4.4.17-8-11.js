// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-8-11
description: >
    Array.prototype.some returns false when all calls to callbackfn
    return false
---*/

  var callCnt = 0;
  function callbackfn(val, idx, obj)
  {
    callCnt++;
    return false;
  }

  var arr = [0,1,2,3,4,5,6,7,8,9];
  

assert.sameValue(arr.some(callbackfn), false, 'arr.some(callbackfn)');
assert.sameValue(callCnt, 10, 'callCnt');
