// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-1-5
description: Array.prototype.indexOf applied to number primitive
---*/

        var targetObj = {};

            Number.prototype[1] = targetObj;
            Number.prototype.length = 2;

assert.sameValue(Array.prototype.indexOf.call(5, targetObj), 1, 'Array.prototype.indexOf.call(5, targetObj)');
