// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-5-24
description: Array.prototype.forEach - string primitive can be used as thisArg
---*/

        var result = false;
        function callbackfn(val, idx, obj) {
            result = (this.valueOf() === "abc");
        }

        [11].forEach(callbackfn, "abc");

assert(result, 'result !== true');
