// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-1-3
description: Array.prototype.indexOf applied to boolean primitive
---*/

        var targetObj = {};

            Boolean.prototype[1] = targetObj;
            Boolean.prototype.length = 2;

assert.sameValue(Array.prototype.indexOf.call(true, targetObj), 1, 'Array.prototype.indexOf.call(true, targetObj)');
