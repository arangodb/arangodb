// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-8-c-5
description: >
    Array.prototype.reduceRight - side effects produced by step 2 are
    visible when an exception occurs
---*/

        var obj = { };

        var accessed = false;

        Object.defineProperty(obj, "length", {
            get: function () {
                accessed = true;
                return 2;
            },
            configurable: true
        });
assert.throws(TypeError, function() {
            Array.prototype.reduceRight.call(obj, function () { });
});
assert(accessed, 'accessed !== true');
