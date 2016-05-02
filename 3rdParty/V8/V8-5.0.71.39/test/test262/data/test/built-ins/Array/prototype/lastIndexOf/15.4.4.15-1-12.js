// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-1-12
description: Array.prototype.lastIndexOf applied to RegExp object
---*/

        var obj = new RegExp("afdasf");
        obj.length = 100;
        obj[1] = "afdasf";

assert.sameValue(Array.prototype.lastIndexOf.call(obj, "afdasf"), 1, 'Array.prototype.lastIndexOf.call(obj, "afdasf")');
