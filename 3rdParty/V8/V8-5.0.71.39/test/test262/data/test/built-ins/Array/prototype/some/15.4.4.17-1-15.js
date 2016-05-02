// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.17-1-15
description: Array.prototype.some applied to the Arguments object
---*/

        function callbackfn(val, idx, obj) {
            return '[object Arguments]' === Object.prototype.toString.call(obj);
        }

        var obj = (function () {
            return arguments;
        }("a", "b"));

assert(Array.prototype.some.call(obj, callbackfn), 'Array.prototype.some.call(obj, callbackfn) !== true');
