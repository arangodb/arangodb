// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-1-7
description: Array.prototype.lastIndexOf applied to string primitive
---*/

assert.sameValue(Array.prototype.lastIndexOf.call("abc", "c"), 2, 'Array.prototype.lastIndexOf.call("abc", "c")');
