// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.3.2-0-7
description: Array.isArray returns false if its argument is not an Array
---*/

  var o = new Object();
  o[12] = 13;
  var b = Array.isArray(o);

assert.sameValue(b, false, 'b');
