// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-1-3
description: Array.prototype.every applied to boolean primitive
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
            return obj instanceof Boolean;
        }

            Boolean.prototype[0] = 1;
            Boolean.prototype.length = 1;

assert(Array.prototype.every.call(false, callbackfn), 'Array.prototype.every.call(false, callbackfn) !== true');
assert(accessed, 'accessed !== true');
