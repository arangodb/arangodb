// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-ii-10
description: Array.prototype.map - callbackfn is called with 1 formal parameter
---*/

        function callbackfn(val) {
            return val > 10;
        }

        var testResult = [11].map(callbackfn);

assert.sameValue(testResult[0], true, 'testResult[0]');
