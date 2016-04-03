// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-5-5
description: Array.prototype.every - thisArg is object from object template
---*/

  var res = false;
  function callbackfn(val, idx, obj)
  {
    return this.res;
  }

  function foo(){}
  var f = new foo();
  f.res = true;
  var arr = [1];

assert.sameValue(arr.every(callbackfn,f), true, 'arr.every(callbackfn,f)');
