// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.21-1-8
description: Array.prototype.reduce applied to String object
---*/

        function callbackfn(prevVal, curVal, idx, obj) {
            return obj instanceof String;
        }

        var obj = new String("abc");

assert(Array.prototype.reduce.call(obj, callbackfn, 1), 'Array.prototype.reduce.call(obj, callbackfn, 1) !== true');
