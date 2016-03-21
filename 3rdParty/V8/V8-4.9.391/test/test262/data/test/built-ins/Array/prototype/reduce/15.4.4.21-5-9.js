// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-5-9
description: >
    Array.prototype.reduce - 'initialValue' is returned if 'len' is 0
    and 'initialValue' is present
---*/

        var accessed = false;
        function callbackfn(prevVal, curVal, idx, obj) {
            accessed = true;
        }

assert.sameValue([].reduce(callbackfn, 3), 3, '[].reduce(callbackfn, 3)');
assert.sameValue(accessed, false, 'accessed');
