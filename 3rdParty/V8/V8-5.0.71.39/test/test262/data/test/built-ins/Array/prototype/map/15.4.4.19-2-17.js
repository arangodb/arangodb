// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-2-17
description: >
    Array.prototype.map - applied to Arguments object, which
    implements its own property get method
---*/

        function callbackfn(val, idx, obj) {
            return val > 10;
        }

        var func = function (a, b) {
            return Array.prototype.map.call(arguments, callbackfn);
        };

        var testResult = func(12, 11);

assert.sameValue(testResult.length, 2, 'testResult.length');
