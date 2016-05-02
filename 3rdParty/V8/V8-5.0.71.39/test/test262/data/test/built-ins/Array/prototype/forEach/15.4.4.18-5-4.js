// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-5-4
description: >
    Array.prototype.forEach - thisArg is object from object
    template(prototype)
---*/

  var res = false;
  var result;
  function callbackfn(val, idx, obj)
  {
    result = this.res;
  }
  
  function foo(){}
  foo.prototype.res = true;
  var f = new foo();
  var arr = [1];
  arr.forEach(callbackfn,f)

assert.sameValue(result, true, 'result');
