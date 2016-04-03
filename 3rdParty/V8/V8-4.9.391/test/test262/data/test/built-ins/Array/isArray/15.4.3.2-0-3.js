// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.3.2-0-3
description: Array.isArray return true if its argument is an Array
---*/

  var a = [];
  var b = Array.isArray(a);

assert.sameValue(b, true, 'b');
