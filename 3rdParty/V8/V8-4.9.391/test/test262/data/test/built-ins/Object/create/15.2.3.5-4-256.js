// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.5-4-256
description: >
    Object.create - one property in 'Properties' is the global object
    that uses Object's [[Get]] method to access the 'get' property
    (8.10.5 step 7.a)
includes: [fnGlobalObject.js]
---*/

        fnGlobalObject().get = function () {
            return "VerifyGlobalObject";
        };

            var newObj = Object.create({}, {
                prop: fnGlobalObject()
            });

assert.sameValue(newObj.prop, "VerifyGlobalObject", 'newObj.prop');
