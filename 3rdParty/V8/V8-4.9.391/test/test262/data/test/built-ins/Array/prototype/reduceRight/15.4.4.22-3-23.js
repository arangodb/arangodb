// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.22-3-23
description: >
    Array.prototype.reduceRight uses inherited valueOf method when
    'length' is an object with an own toString and inherited valueOf
    methods
---*/

        var testResult1 = true;
        var testResult2 = false;
        var valueOfAccessed = false;
        var toStringAccessed = false;

        var proto = {
            valueOf: function () {
                valueOfAccessed = true;
                return 2;
            }
        };
        var Con = function () { };
        Con.prototype = proto;
        var child = new Con();

        function callbackfn(prevVal, curVal, idx, obj) {
            if (idx > 1) {
                testResult1 = false;
            }

            if (idx === 1) {
                testResult2 = true;
            }
            return false;
        }

        child.toString = function () {
            toStringAccessed = true;
            return '1';
        };

        var obj = {
            0: 12,
            1: 11,
            2: 9,
            length: child
        };

        Array.prototype.reduceRight.call(obj, callbackfn, 1);

assert(testResult1, 'testResult1 !== true');
assert(testResult2, 'testResult2 !== true');
assert(valueOfAccessed, 'valueOfAccessed !== true');
assert.sameValue(toStringAccessed, false, 'toStringAccessed');
