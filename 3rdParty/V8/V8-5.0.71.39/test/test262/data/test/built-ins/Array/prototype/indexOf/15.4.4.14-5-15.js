// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-5-15
description: >
    Array.prototype.indexOf - value of 'fromIndex' is a string
    containing a negative number
---*/

assert.sameValue([0, true, 2].indexOf(true, "-1"), -1, '[0, true, 2].indexOf(true, "-1")');
assert.sameValue([0, 1, true].indexOf(true, "-1"), 2, '[0, 1, true].indexOf(true, "-1")');
