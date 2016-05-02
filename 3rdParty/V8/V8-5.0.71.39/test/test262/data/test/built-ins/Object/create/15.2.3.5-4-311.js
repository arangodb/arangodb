// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.5-4-311
description: >
    Object.create - [[Set]] is set as undefined if it is absent in
    accessor descriptor of one property in 'Properties' (8.12.9 step
    4.b)
---*/

        var newObj = Object.create({}, {
            prop: {
                get: function () {
                    return "verifyCreate";
                },
                enumerable: true,
                configurable: true
            }
        });

        var desc = Object.getOwnPropertyDescriptor(newObj, "prop");
        var verifySet = desc.hasOwnProperty("set") && typeof desc.set === "undefined";

        var verifyGet = false;
        if (newObj.prop === "verifyCreate") {
            verifyGet = true;
        }

        var verifyEnumerable = false;
        for (var p in newObj) {
            if (p === "prop") {
                verifyEnumerable = true;
            }
        }

        var verifyConfigurable = false;
        var hasProperty = newObj.hasOwnProperty("prop");
        delete newObj.prop;
        verifyConfigurable = !newObj.hasOwnProperty("prop") && hasProperty;

assert(verifySet, 'verifySet !== true');
assert(verifyGet, 'verifyGet !== true');
assert(verifyEnumerable, 'verifyEnumerable !== true');
assert(verifyConfigurable, 'verifyConfigurable !== true');
