// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-9-c-ii-9
description: >
    Array.prototype.reduce - callbackfn is called with 0 formal
    parameter
---*/

        var called = 0;
        function callbackfn() {
            called++;
        }

        [11, 12].reduce(callbackfn, 1);

assert.sameValue(called, 2, 'called');
