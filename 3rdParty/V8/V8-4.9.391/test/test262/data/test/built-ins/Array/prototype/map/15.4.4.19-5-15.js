// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-5-15
description: Array.prototype.map - Date object can be used as thisArg
---*/

        var objDate = new Date();

        function callbackfn(val, idx, obj) {
            return this === objDate;
        }

        var testResult = [11].map(callbackfn, objDate);

assert.sameValue(testResult[0], true, 'testResult[0]');
