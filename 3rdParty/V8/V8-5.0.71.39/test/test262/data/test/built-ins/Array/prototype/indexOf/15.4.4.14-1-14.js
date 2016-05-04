// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-1-14
description: Array.prototype.indexOf applied to Error object
---*/

        var obj = new SyntaxError();
        obj[1] = true;
        obj.length = 2;

assert.sameValue(Array.prototype.indexOf.call(obj, true), 1, 'Array.prototype.indexOf.call(obj, true)');
