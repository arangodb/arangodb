// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-ii-13
description: >
    Array.prototype.map - callbackfn that uses arguments object to get
    parameter value
---*/

        function callbackfn() {
            return arguments[2][arguments[1]] === arguments[0];
        }

        var testResult = [11].map(callbackfn);

assert.sameValue(testResult[0], true, 'testResult[0]');
