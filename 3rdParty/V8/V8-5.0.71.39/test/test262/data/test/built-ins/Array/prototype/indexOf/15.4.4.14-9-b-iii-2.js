// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-b-iii-2
description: >
    Array.prototype.indexOf - returns without visiting subsequent
    element once search value is found
---*/

        var arr = [1, 2, , 1, 2];
        var elementThirdAccessed = false;
        var elementFifthAccessed = false;

        Object.defineProperty(arr, "2", {
            get: function () {
                elementThirdAccessed = true;
                return 2;
            },
            configurable: true
        });
        Object.defineProperty(arr, "4", {
            get: function () {
                elementFifthAccessed = true;
                return 2;
            },
            configurable: true
        });

        arr.indexOf(2);

assert.sameValue(elementThirdAccessed, false, 'elementThirdAccessed');
assert.sameValue(elementFifthAccessed, false, 'elementFifthAccessed');
