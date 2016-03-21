// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-2-2
description: Array.prototype.reduce - 'length' is own data property on an Array
---*/

        function callbackfn(prevVal, curVal, idx, obj) {
            return (obj.length === 2);
        }

assert.sameValue([12, 11].reduce(callbackfn, 1), true, '[12, 11].reduce(callbackfn, 1)');
