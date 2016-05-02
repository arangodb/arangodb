// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-a-14
description: >
    Array.prototype.indexOf - deleting property of prototype causes
    prototype index property not to be visited on an Array
---*/

        var arr = [0, , 2];

        Object.defineProperty(arr, "0", {
            get: function () {
                delete Array.prototype[1];
                return 0;
            },
            configurable: true
        });

            Array.prototype[1] = 1;

assert.sameValue(arr.indexOf(1), -1, 'arr.indexOf(1)');
