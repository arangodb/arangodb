// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-9-b-i-20
description: >
    Array.prototype.indexOf - element to be retrieved is own accessor
    property without a get function that overrides an inherited
    accessor property on an Array-like object
---*/

        var proto = {};
        Object.defineProperty(proto, "0", {
            get: function () {
                return 2;
            },
            configurable: true
        });

        var Con = function () { };
        Con.prototype = proto;

        var child = new Con();
        child.length = 1;

        Object.defineProperty(child, "0", {
            set: function () { },
            configurable: true
        });

assert.sameValue(Array.prototype.indexOf.call(child, undefined), 0, 'Array.prototype.indexOf.call(child, undefined)');
