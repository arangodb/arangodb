// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-8-13
description: Array.prototype.every doesn't visit expandos
---*/

  var callCnt = 0;
  function callbackfn(val, idx, obj)
  {
    callCnt++;
    return true;
  }

  var arr = [0,1,2,3,4,5,6,7,8,9];
  arr["i"] = 10;
  arr[true] = 11;
  

assert.sameValue(arr.every(callbackfn), true, 'arr.every(callbackfn)');
assert.sameValue(callCnt, 10, 'callCnt');
