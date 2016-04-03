// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-105
description: >
    Object.defineProperties - 'P' is accessor property, P.[[Set]] is
    present and properties.[[Set]] is undefined (8.12.9 step 12)
---*/

        var obj = {};
        function get_func() {
            return 10;
        }
        function set_func() {
            return 10;
        }

        Object.defineProperty(obj, "property", {
            get: get_func,
            set: set_func,
            enumerable: true,
            configurable: true
        });

        Object.defineProperties(obj, {
            property: {
                set: undefined
            }
        });

        var hasProperty = obj.hasOwnProperty("property");
        var verifyGet = false;
        verifyGet = (obj.property === 10);

        var verifySet = false;
        var desc = Object.getOwnPropertyDescriptor(obj, "property");
        verifySet = (typeof desc.set === 'undefined');

        var verifyEnumerable = false;
        for (var p in obj) {
            if (p === "property") {
                verifyEnumerable = true;
            }
        }

        var verifyConfigurable = false;
        delete obj.property;
        verifyConfigurable = obj.hasOwnProperty("property");

assert(hasProperty, 'hasProperty !== true');
assert(verifyGet, 'verifyGet !== true');
assert(verifySet, 'verifySet !== true');
assert(verifyEnumerable, 'verifyEnumerable !== true');
assert.sameValue(verifyConfigurable, false, 'verifyConfigurable');
