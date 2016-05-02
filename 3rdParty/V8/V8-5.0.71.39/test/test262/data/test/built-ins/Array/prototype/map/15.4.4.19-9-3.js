// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-9-3
description: Array.prototype.map - subclassed array when length is reduced
---*/

  foo.prototype = new Array(1, 2, 3);
  function foo() {}
  var f = new foo();
  f.length = 1;
  
  function cb(){}
  var a = f.map(cb);
  

assert(Array.isArray(a), 'Array.isArray(a) !== true');
assert.sameValue(a.length, 1, 'a.length');
