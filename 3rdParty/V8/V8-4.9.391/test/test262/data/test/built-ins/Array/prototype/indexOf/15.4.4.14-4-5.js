// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-4-5
description: >
    Array.prototype.indexOf returns -1 if 'length' is 0 ( length
    overridden to '0' (type conversion))
---*/

 var i = Array.prototype.indexOf.call({length: '0'}, 1);
  

assert.sameValue(i, -1, 'i');
