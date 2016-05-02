// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-7
description: Array.prototype.indexOf must return correct index (self reference)
---*/

  var a = new Array(0,1,2,3);  
  a[2] = a;

assert.sameValue(a.indexOf(a), 2, 'a.indexOf(a)');
assert.sameValue(a.indexOf(3), 3, 'a.indexOf(3)');
