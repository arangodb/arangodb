// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.9.5.43-0-14
description: >
    Date.prototype.toISOString - when value of year is -Infinity
    Date.prototype.toISOString throw the RangeError
---*/

        var date = new Date(-Infinity, 1, 70, 0, 0, 0);
assert.throws(RangeError, function() {
            date.toISOString();
});
