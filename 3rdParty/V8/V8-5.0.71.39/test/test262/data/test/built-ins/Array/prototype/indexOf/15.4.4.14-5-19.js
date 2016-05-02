// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-5-19
description: >
    Array.prototype.indexOf - value of 'fromIndex' is a string
    containing a hex number
---*/

        var targetObj = {};

assert.sameValue([0, 1, targetObj, 3, 4].indexOf(targetObj, "0x0003"), -1, '[0, 1, targetObj, 3, 4].indexOf(targetObj, "0x0003")');
assert.sameValue([0, 1, 2, targetObj, 4].indexOf(targetObj, "0x0003"), 3, '[0, 1, 2, targetObj, 4].indexOf(targetObj, "0x0003")');
