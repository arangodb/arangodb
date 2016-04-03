// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-7-2
description: >
    Array.prototype.every considers new value of elements in array
    after the call
---*/

  function callbackfn(val, Idx, obj)
  {
    arr[4] = 6;
    if(val < 6)
       return true;
    else 
       return false;
  }

  var arr = [1,2,3,4,5];
  

assert.sameValue(arr.every(callbackfn), false, 'arr.every(callbackfn)');
