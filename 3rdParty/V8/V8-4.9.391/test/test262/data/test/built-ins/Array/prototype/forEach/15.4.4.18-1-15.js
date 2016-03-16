// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-1-15
description: Array.prototype.forEach applied to the Arguments object
---*/

        var result = false;
        function callbackfn(val, idx, obj) {
            result = ('[object Arguments]' === Object.prototype.toString.call(obj));
        }

        var obj = (function () {
            return arguments;
        }("a", "b"));

        Array.prototype.forEach.call(obj, callbackfn);

assert(result, 'result !== true');
