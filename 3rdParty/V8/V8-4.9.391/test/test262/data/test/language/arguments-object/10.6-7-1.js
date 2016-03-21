// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.6-7-1
description: >
    Arguments Object has length as its own property and does not
    invoke the setter defined on Object.prototype.length (Step 7)
---*/

            var data = "data";
            var getFunc = function () {
                return 12;
            };

            var setFunc = function (value) {
                data = value;
            };

            Object.defineProperty(Object.prototype, "length", {
                get: getFunc,
                set: setFunc,
                configurable: true
            });

            var verifyValue = false;
            var argObj = (function () { return arguments })();
            verifyValue = (argObj.length === 0);

            var verifyWritable = false;
            argObj.length = 1001;
            verifyWritable = (argObj.length === 1001);

            var verifyEnumerable = false;
            for (var p in argObj) {
                if (p === "length") {
                    verifyEnumerable = true;
                }
            }

            var verifyConfigurable = false;
            delete argObj.length;
            verifyConfigurable = argObj.hasOwnProperty("length");

assert(verifyValue, 'verifyValue !== true');
assert(verifyWritable, 'verifyWritable !== true');
assert.sameValue(verifyEnumerable, false, 'verifyEnumerable');
assert.sameValue(verifyConfigurable, false, 'verifyConfigurable');
assert.sameValue(data, "data", 'data');
