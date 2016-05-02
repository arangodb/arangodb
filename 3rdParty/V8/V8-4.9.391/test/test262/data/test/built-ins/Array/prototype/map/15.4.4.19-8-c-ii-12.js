// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-8-c-ii-12
description: Array.prototype.map - callbackfn is called with 3 formal parameters
---*/

        function callbackfn(val, idx, obj) {
            return (val > 10 && obj[idx] === val);
        }

        var testResult = [11].map(callbackfn);

assert.sameValue(testResult[0], true, 'testResult[0]');
