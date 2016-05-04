// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-a-3
description: >
    Array.prototype.indexOf - added properties in step 5 are visible
    here on an Array
---*/

        var arr = [];
        arr.length = 30;
        var targetObj = function () { };

        var fromIndex = {
            valueOf: function () {
                arr[4] = targetObj;
                return 3;
            }
        };

assert.sameValue(arr.indexOf(targetObj, fromIndex), 4, 'arr.indexOf(targetObj, fromIndex)');
