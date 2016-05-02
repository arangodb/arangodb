// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-8-2
description: >
    Array.prototype.every returns true if 'length' is 0 (subclassed
    Array, length overridden to null (type conversion))
---*/

  foo.prototype = new Array(1, 2, 3);
  function foo() {}
  var f = new foo();
  f.length = null;
  
  function cb(){}
  var i = f.every(cb);
  

assert.sameValue(i, true, 'i');
