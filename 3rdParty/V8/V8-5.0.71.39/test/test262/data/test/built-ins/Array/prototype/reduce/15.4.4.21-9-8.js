// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-9-8
description: Array.prototype.reduce - no observable effects occur if 'len' is 0
---*/

        var accessed = false;
        var callbackAccessed = false;
        function callbackfn() {
            callbackAccessed = true;
        }

        var obj = { length: 0 };

        Object.defineProperty(obj, "0", {
            get: function () {
                accessed = true;
                return 10;
            },
            configurable: true
        });

        Array.prototype.reduce.call(obj, function () { }, "initialValue");

assert.sameValue(accessed, false, 'accessed');
assert.sameValue(callbackAccessed, false, 'callbackAccessed');
