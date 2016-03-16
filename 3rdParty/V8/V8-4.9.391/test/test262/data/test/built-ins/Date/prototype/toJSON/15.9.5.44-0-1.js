// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.9.5.44-0-1
description: Date.prototype.toJSON must exist as a function
---*/

  var f = Date.prototype.toJSON;

assert.sameValue(typeof(f), "function", 'typeof(f)');
