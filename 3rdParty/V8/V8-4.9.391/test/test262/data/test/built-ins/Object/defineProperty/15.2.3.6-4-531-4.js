// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-531-4
description: >
    Object.defineProperty will update [[Get]] and [[Set]] attributes
    of named accessor property 'P' successfully when [[Configurable]]
    attribute is true, 'O' is the global object (8.12.9 step 11)
includes: [propertyHelper.js, fnGlobalObject.js]
---*/


var obj = fnGlobalObject();
try {
    obj.verifySetFunction = "data";
    Object.defineProperty(obj, "property", {
        get: function () {
            return obj.verifySetFunction;
        },
        set: function (value) {
            obj.verifySetFunction = value;
        },
        configurable: true
    });

    obj.verifySetFunction1 = "data1";
    var getFunc = function () {
        return obj.verifySetFunction1;
    };
    var setFunc = function (value) {
        obj.verifySetFunction1 = value;
    };

    Object.defineProperty(obj, "property", {
        get: getFunc,
        set: setFunc
    });

    verifyEqualTo(obj, "property", getFunc());

    verifyWritable(obj, "property", "verifySetFunction1");

    verifyNotEnumerable(obj, "property");

    verifyConfigurable(obj, "property");
} finally {
    delete obj.property;
    delete obj.verifySetFunction;
    delete obj.verifySetFunction1;
}
