// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-3-19
description: >
    Array.prototype.lastIndexOf - value of 'length' is an Object which
    has an own toString method
includes: [fnGlobalObject.js]
---*/

        // objects inherit the default valueOf() method from Object
        // that simply returns itself. Since the default valueOf() method
        // does not return a primitive value, ES next tries to convert the object
        // to a number by calling its toString() method and converting the
        // resulting string to a number.

        var targetObj = fnGlobalObject();
        var obj = {
            1: targetObj,
            2: 2,

            length: {
                toString: function () {
                    return '2';
                }
            }
        };

assert.sameValue(Array.prototype.lastIndexOf.call(obj, targetObj), 1, 'Array.prototype.lastIndexOf.call(obj, targetObj)');
assert.sameValue(Array.prototype.lastIndexOf.call(obj, 2), -1, 'Array.prototype.lastIndexOf.call(obj, 2)');
