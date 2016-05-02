// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-202
description: >
    Object.defineProperties - 'O' is an Array, 'P' is an array index
    named property, 'P' property doesn't exist in 'O', test [[Set]] of
    'P' property in 'Attributes' is set as undefined value if [[Set]]
    is absent in accessor descriptor 'desc'  (15.4.5.1 step 4.c)
---*/

        var arr = [];
        var getFunc = function () {
            return 11;
        };

        Object.defineProperties(arr, {
            "0": {
                get: getFunc,
                enumerable: true,
                configurable: true
            }
        });

        var verifyEnumerable = false;
        for (var i in arr) {
            if (i === "0" && arr.hasOwnProperty("0")) {
                verifyEnumerable = true;
            }
        }

        var desc = Object.getOwnPropertyDescriptor(arr, "0");
        var propertyDefineCorrect = arr.hasOwnProperty("0");

        var verifyConfigurable = false;
        delete arr[0];
        verifyConfigurable = arr.hasOwnProperty("0");

assert.sameValue(typeof desc.set, "undefined", 'typeof desc.set');
assert(propertyDefineCorrect, 'propertyDefineCorrect !== true');
assert.sameValue(desc.get, getFunc, 'desc.get');
assert.sameValue(verifyConfigurable, false, 'verifyConfigurable');
assert(verifyEnumerable, 'verifyEnumerable !== true');
