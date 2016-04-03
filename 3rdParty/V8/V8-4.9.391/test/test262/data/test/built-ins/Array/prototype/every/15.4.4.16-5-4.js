// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-5-4
description: >
    Array.prototype.every - thisArg is object from object
    template(prototype)
---*/

  var res = false;
  function callbackfn(val, idx, obj)
  {
    return this.res;
  }
  
  function foo(){}
  foo.prototype.res = true;
  var f = new foo();
  var arr = [1];

assert.sameValue(arr.every(callbackfn,f), true, 'arr.every(callbackfn,f)');
