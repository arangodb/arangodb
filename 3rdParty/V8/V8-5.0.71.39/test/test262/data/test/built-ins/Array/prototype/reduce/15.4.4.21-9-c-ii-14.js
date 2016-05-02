// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-9-c-ii-14
description: Array.prototype.reduce - callbackfn that uses arguments
---*/

        var result = false;
        function callbackfn() {
            result = (arguments[0] === 1 && arguments[3][arguments[2]] === arguments[1]);
        }

        [11].reduce(callbackfn, 1);

assert(result, 'result !== true');
