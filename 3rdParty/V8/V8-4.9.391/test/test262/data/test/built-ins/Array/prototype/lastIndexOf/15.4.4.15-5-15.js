// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-5-15
description: >
    Array.prototype.lastIndexOf - value of 'fromIndex' is a string
    containing a negative number
---*/

assert.sameValue([0, "-2", 2].lastIndexOf("-2", "-2"), 1, '[0, "-2", 2].lastIndexOf("-2", "-2")');
assert.sameValue([0, 2, "-2"].lastIndexOf("-2", "-2"), -1, '[0, 2, "-2"].lastIndexOf("-2", "-2")');
