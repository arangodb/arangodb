// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-5-b-109
description: >
    Object.defineProperties - value of 'configurable' property of
    'descObj' is  the global object (8.10.5 step 4.b)
includes: [fnGlobalObject.js]
---*/

        var obj = {};

        Object.defineProperties(obj, {
            property: {
                configurable: fnGlobalObject()
            }
        });
        var preCheck = obj.hasOwnProperty("property");
        delete obj.property;

assert(preCheck, 'preCheck !== true');
assert.sameValue(obj.hasOwnProperty("property"), false, 'obj.hasOwnProperty("property")');
