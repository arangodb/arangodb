// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.18-7-c-i-21
description: >
    Array.prototype.forEach - element to be retrieved is inherited
    accessor property without a get function on an Array-like object
---*/

        var testResult = false;

        function callbackfn(val, idx, obj) {
            if (idx === 1) {
                testResult = (typeof val === "undefined");
            }
        }

        var proto = {};
        Object.defineProperty(proto, "1", {
            set: function () { },
            configurable: true
        });

        var Con = function () { };
        Con.prototype = proto;

        var child = new Con();
        child.length = 2;

        Array.prototype.forEach.call(child, callbackfn);

assert(testResult, 'testResult !== true');
