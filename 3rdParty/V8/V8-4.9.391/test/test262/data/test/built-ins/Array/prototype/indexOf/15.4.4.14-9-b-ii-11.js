// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-b-ii-11
description: >
    Array.prototype.indexOf - both array element and search element
    are Object type, and they refer to the same object
---*/

        var obj1 = {};
        var obj2 = {};
        var obj3 = obj2;

assert.sameValue([{}, obj1, obj2].indexOf(obj3), 2, '[{}, obj1, obj2].indexOf(obj3)');
