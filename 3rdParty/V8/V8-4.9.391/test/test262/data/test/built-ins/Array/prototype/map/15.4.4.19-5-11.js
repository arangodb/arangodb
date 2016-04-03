// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-5-11
description: Array.prototype.map - String object can be used as thisArg
---*/

        var objString = new String();

        function callbackfn(val, idx, obj) {
            return this === objString;
        }

        var testResult = [11].map(callbackfn, objString);

assert.sameValue(testResult[0], true, 'testResult[0]');
