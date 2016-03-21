// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-ii-6
description: Array.prototype.map - arguments to callbackfn are self consistent.
---*/

        var obj = { 0: 11, length: 1 };
        var thisArg = {};

        function callbackfn() {
            return this === thisArg &&
                arguments[0] === 11 &&
                arguments[1] === 0 &&
                arguments[2] === obj;
        }

        var testResult = Array.prototype.map.call(obj, callbackfn, thisArg);

assert.sameValue(testResult[0], true, 'testResult[0]');
