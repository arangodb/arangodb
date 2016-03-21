// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-3-23
description: >
    Array.prototype.indexOf uses inherited valueOf method when
    'length' is an object with an own toString and inherited valueOf
    methods
---*/

        var toStringAccessed = false;
        var valueOfAccessed = false;

        var proto = {
            valueOf: function () {
                valueOfAccessed = true;
                return 2;
            }
        };

        var Con = function () {};
        Con.prototype = proto;

        var child = new Con();
        child.toString = function () {
            toStringAccessed = true;
            return 2;
        };

        var obj = {
            1: true,
            length: child
        };

assert.sameValue(Array.prototype.indexOf.call(obj, true), 1, 'Array.prototype.indexOf.call(obj, true)');
assert(valueOfAccessed, 'valueOfAccessed !== true');
assert.sameValue(toStringAccessed, false, 'toStringAccessed');
