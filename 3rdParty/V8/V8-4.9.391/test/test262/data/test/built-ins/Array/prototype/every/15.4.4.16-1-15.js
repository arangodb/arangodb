// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.16-1-15
description: Array.prototype.every applied to the Arguments object
---*/

        function callbackfn(val, idx, obj) {
            return ('[object Arguments]' !== Object.prototype.toString.call(obj));
        }

        var obj = (function fun() {
            return arguments;
        }("a", "b"));

assert.sameValue(Array.prototype.every.call(obj, callbackfn), false, 'Array.prototype.every.call(obj, callbackfn)');
