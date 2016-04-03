// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-a-17
description: >
    Array.prototype.indexOf - decreasing length of array causes index
    property not to be visited
---*/

        var arr = [0, 1, 2, "last"];

        Object.defineProperty(arr, "0", {
            get: function () {
                arr.length = 3;
                return 0;
            },
            configurable: true
        });

assert.sameValue(arr.indexOf("last"), -1, 'arr.indexOf("last")');
