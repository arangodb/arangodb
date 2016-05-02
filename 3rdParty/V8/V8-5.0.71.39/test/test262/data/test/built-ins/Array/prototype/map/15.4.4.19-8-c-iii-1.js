// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-iii-1
description: >
    Array.prototype.map - getOwnPropertyDescriptor(all true) of
    returned array element
---*/
  
  function callbackfn(val, idx, obj){
	  if(val % 2)
	    return (2 * val + 1); 
	  else
	    return (val / 2);
  }
  var srcArr = [0,1,2,3,4];
  var resArr = srcArr.map(callbackfn);

assert(resArr.length > 0, 'resArr.length > 0');

var desc = Object.getOwnPropertyDescriptor(resArr, 1);

assert.sameValue(desc.value, 3, 'desc.value'); //srcArr[1] = 2*1+1 = 3
assert.sameValue(desc.writable, true, 'desc.writable');
assert.sameValue(desc.enumerable, true, 'desc.enumerable');
assert.sameValue(desc.configurable, true, 'desc.configurable');
