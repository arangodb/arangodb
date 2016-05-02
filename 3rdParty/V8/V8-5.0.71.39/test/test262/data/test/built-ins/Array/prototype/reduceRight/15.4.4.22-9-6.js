// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-6
description: >
    Array.prototype.reduceRight visits deleted element in array after
    the call when same index is also present in prototype
---*/

  function callbackfn(prevVal, curVal, idx, obj)  
  {
    delete arr[1];
    delete arr[2];
    return prevVal + curVal;    
  }
  Array.prototype[2] = 6;
  var arr = ['1',2,3,4,5];
  var res = arr.reduceRight(callbackfn);
  delete Array.prototype[2];

//one element deleted
assert.sameValue(res, "151", 'res');
