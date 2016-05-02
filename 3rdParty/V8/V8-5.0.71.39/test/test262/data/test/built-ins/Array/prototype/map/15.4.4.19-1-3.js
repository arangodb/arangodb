// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-1-3
description: Array.prototype.map - applied to boolean primitive
---*/

        function callbackfn(val, idx, obj) {
            return obj instanceof Boolean;
        }

            Boolean.prototype[0] = true;
            Boolean.prototype.length = 1;

            var testResult = Array.prototype.map.call(false, callbackfn);

assert.sameValue(testResult[0], true, 'testResult[0]');
