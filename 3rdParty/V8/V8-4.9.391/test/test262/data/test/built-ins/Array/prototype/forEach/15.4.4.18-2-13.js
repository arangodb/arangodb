// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-2-13
description: >
    Array.prototype.forEach applied to the Array-like object that
    'length' is inherited accessor property without a get function
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
        }

        var proto = {};
        Object.defineProperty(proto, "length", {
            set: function () { },
            configurable: true
        });

        var Con = function () { };
        Con.prototype = proto;

        var child = new Con();
        child[0] = 11;
        child[1] = 12;

        Array.prototype.forEach.call(child, callbackfn);

assert.sameValue(accessed, false, 'accessed');
