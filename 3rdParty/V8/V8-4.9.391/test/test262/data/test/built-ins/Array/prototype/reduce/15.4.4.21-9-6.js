// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-9-6
description: >
    Array.prototype.reduce visits deleted element in array after the
    call when same index is also present in prototype
---*/

  function callbackfn(prevVal, curVal, idx, obj)  
  {
    delete arr[3];
    delete arr[4];
    return prevVal + curVal;    
  }

  Array.prototype[4] = 5;
  var arr = ['1',2,3,4,5];
  var res = arr.reduce(callbackfn);
  delete Array.prototype[4];

//one element acually deleted
assert.sameValue(res, "1235", 'res');
