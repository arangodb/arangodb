// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.4-5-b-iii-3-b-1
description: >
    Array.prototype.concat will concat an Array when index property
    (read-only) exists in Array.prototype (Step 5.b.iii.3.b)
---*/

            Object.defineProperty(Array.prototype, "0", {
                value: 100,
                writable: false,
                configurable: true
            });

            var oldArr = [101];

            var newArr = Array.prototype.concat.call(oldArr);

            var verifyValue = false;
            verifyValue = newArr[0] === 101;

            var verifyEnumerable = false;
            for (var p in newArr) {
                if (p === "0" && newArr.hasOwnProperty("0")) {
                    verifyEnumerable = true;
                }
            }

            var verifyWritable = false;
            newArr[0] = 12;
            verifyWritable = newArr[0] === 12;

            var verifyConfigurable = false;
            delete newArr[0];
            verifyConfigurable = newArr.hasOwnProperty("0");

assert(verifyValue, 'verifyValue !== true');
assert.sameValue(verifyConfigurable, false, 'verifyConfigurable');
assert(verifyEnumerable, 'verifyEnumerable !== true');
assert(verifyWritable, 'verifyWritable !== true');
