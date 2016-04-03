// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.19-1-7
description: Array.prototype.map - applied to string primitive
---*/

        function callbackfn(val, idx, obj) {
            return obj instanceof String;
        }

        var testResult = Array.prototype.map.call("abc", callbackfn);

assert.sameValue(testResult[0], true, 'testResult[0]');
assert.sameValue(testResult[1], true, 'testResult[1]');
assert.sameValue(testResult[2], true, 'testResult[2]');
