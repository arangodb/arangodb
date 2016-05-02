// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-5-4
description: >
    Array.prototype.map - thisArg is object from object
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
  
  var srcArr = [1];
  var resArr = srcArr.map(callbackfn,f);

assert.sameValue(resArr[0], true, 'resArr[0]');
