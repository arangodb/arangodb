// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-0-1
description: Array.prototype.map must exist as a function
---*/

  var f = Array.prototype.map;

assert.sameValue(typeof(f), "function", 'typeof(f)');
