// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.20-5-2
description: Array.prototype.filter - thisArg is Object
---*/

  var res = false;
  var o = new Object();
  o.res = true;
  function callbackfn(val, idx, obj)
  {
    return this.res;
  }

  var srcArr = [1];
  var resArr = srcArr.filter(callbackfn,o);

assert.sameValue(resArr.length, 1, 'resArr.length');
