// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-5-16
description: Array.prototype.map - RegExp object can be used as thisArg
---*/

        var objRegExp = new RegExp();

        function callbackfn(val, idx, obj) {
            return this === objRegExp;
        }

        var testResult = [11].map(callbackfn, objRegExp);

assert.sameValue(testResult[0], true, 'testResult[0]');
