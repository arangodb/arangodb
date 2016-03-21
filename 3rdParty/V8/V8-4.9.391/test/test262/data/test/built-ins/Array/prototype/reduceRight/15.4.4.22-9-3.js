// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-9-3
description: >
    Array.prototype.reduceRight doesn't consider unvisited deleted
    elements in array after the call
---*/

  function callbackfn(prevVal, curVal, idx, obj)  
  {
    delete arr[1];
    delete arr[4];
    return prevVal + curVal;    
  }

  var arr = ['1',2,3,4,5];

// two elements deleted
assert.sameValue(arr.reduceRight(callbackfn), "121", 'arr.reduceRight(callbackfn)');
