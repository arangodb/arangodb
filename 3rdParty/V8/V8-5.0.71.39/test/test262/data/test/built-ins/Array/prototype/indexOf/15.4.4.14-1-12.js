// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-1-12
description: Array.prototype.indexOf applied to RegExp object
---*/

        var obj = new RegExp();
        obj.length = 2;
        obj[1] = true;

assert.sameValue(Array.prototype.indexOf.call(obj, true), 1, 'Array.prototype.indexOf.call(obj, true)');
