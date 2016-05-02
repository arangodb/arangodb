// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-8-5
description: >
    Array.prototype.some returns false if 'length' is 0 (subclassed
    Array, length overridden to '0' (type conversion))
---*/

  foo.prototype = new Array(1, 2, 3);
  function foo() {}
  var f = new foo();
  f.length = '0';
  
  function cb(){}
  var i = f.some(cb);
  

assert.sameValue(i, false, 'i');
