// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-9-c-1
description: >
    Array.prototype.reduce - callbackfn not called for indexes never
    been assigned values
---*/

  var callCnt = 0;
  function callbackfn(prevVal, curVal, idx, obj)
  {
    callCnt++;
    return curVal;
  }

  var arr = new Array(10);
  arr[0] = arr[1] = undefined; //explicitly assigning a value

assert.sameValue(arr.reduce(callbackfn), undefined, 'arr.reduce(callbackfn)');
assert.sameValue(callCnt, 1, 'callCnt');
