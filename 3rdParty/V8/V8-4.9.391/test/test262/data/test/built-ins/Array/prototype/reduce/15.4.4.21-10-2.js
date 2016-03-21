// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-10-2
description: >
    Array.prototype.reduce reduces the array in ascending order of
    indices
---*/

  function callbackfn(prevVal, curVal,  idx, obj)
  {
    return prevVal + curVal;
  }
  var srcArr = ['1','2','3','4','5'];

assert.sameValue(srcArr.reduce(callbackfn), '12345', 'srcArr.reduce(callbackfn)');
