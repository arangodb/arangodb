// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-b-1
description: >
    Array.prototype.some - callbackfn not called for indexes never
    been assigned values
---*/

  var callCnt = 0; 
  function callbackfn(val, idx, obj)
  {
    callCnt++;
    return false;
  }

  var arr = new Array(10);
  arr[1] = undefined;
  arr.some(callbackfn);

assert.sameValue(callCnt, 1, 'callCnt');
