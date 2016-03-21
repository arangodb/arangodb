// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-5-20
description: >
    Array.prototype.indexOf - value of 'fromIndex' which is a string
    containing a number with leading zeros
---*/

        var targetObj = {};

assert.sameValue([0, 1, targetObj, 3, 4].indexOf(targetObj, "0003.10"), -1, '[0, 1, targetObj, 3, 4].indexOf(targetObj, "0003.10")');
assert.sameValue([0, 1, 2, targetObj, 4].indexOf(targetObj, "0003.10"), 3, '[0, 1, 2, targetObj, 4].indexOf(targetObj, "0003.10")');
