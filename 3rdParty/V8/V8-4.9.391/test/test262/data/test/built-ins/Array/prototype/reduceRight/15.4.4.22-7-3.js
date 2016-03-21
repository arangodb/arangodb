// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-7-3
description: >
    Array.prototype.reduceRight returns initialValue if 'length' is 0
    and initialValue is present (subclassed Array, length overridden
    to false (type conversion))
---*/

  foo.prototype = new Array(1, 2, 3);
  function foo() {}
  var f = new foo();
  f.length = false;
  
  function cb(){}
assert.sameValue(f.reduceRight(cb,1), 1, 'f.reduceRight(cb,1)');
