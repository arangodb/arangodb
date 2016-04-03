// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-7-c-i-30
description: >
    Array.prototype.some - unhandled exceptions happened in getter
    terminate iteration on an Array-like object
---*/

        var accessed = false;
        function callbackfn(val, idx, obj) {
            if (idx > 1) {
                accessed = true;
            }
            return true;
        }

        var obj = { length: 20 };
        Object.defineProperty(obj, "1", {
            get: function () {
                throw new RangeError("unhandle exception happened in getter");
            },
            configurable: true
        });
assert.throws(RangeError, function() {
            Array.prototype.some.call(obj, callbackfn);
});
assert.sameValue(accessed, false, 'accessed');
