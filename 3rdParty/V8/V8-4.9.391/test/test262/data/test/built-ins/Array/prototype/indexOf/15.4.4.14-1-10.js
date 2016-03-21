// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-1-10
description: Array.prototype.indexOf applied to the Math object
---*/

            Math[1] = true;
            Math.length = 2;

assert.sameValue(Array.prototype.indexOf.call(Math, true), 1, 'Array.prototype.indexOf.call(Math, true)');
