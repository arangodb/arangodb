// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-0-1
description: Array.prototype.reduceRight must exist as a function
---*/

  var f = Array.prototype.reduceRight;

assert.sameValue(typeof(f), "function", 'typeof(f)');
