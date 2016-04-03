// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.14-6-6
description: >
    Object.keys - the order of elements in returned array is the same
    with the order of properties in 'O' (global Object)
includes: [fnGlobalObject.js]
---*/

        var obj = fnGlobalObject();

        var tempArray = [];
        for (var p in obj) {
            if (obj.hasOwnProperty(p)) {
                tempArray.push(p);
            }
        }

        var returnedArray = Object.keys(obj);

        for (var index in returnedArray) {
            assert.sameValue(tempArray[index], returnedArray[index], 'tempArray[index]');
        }
