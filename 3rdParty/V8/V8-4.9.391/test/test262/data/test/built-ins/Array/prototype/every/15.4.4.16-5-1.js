// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-5-1
description: Array.prototype.every - thisArg not passed
flags: [noStrict]
includes: [fnGlobalObject.js]
---*/

  function callbackfn(val, idx, obj)
  {
    return this === fnGlobalObject();
  }

  var arr = [1];

assert.sameValue(arr.every(callbackfn), true, 'arr.every(callbackfn)');
