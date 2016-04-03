// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-531-13
description: >
    Object.defineProperty will update [[Get]] and [[Set]] attributes
    of indexed accessor property 'P' successfully when
    [[Configurable]] attribute is true, 'O' is the global object
    (8.12.9 step 11)
includes: [propertyHelper.js, fnGlobalObject.js]
---*/


var obj = fnGlobalObject();
try {
    obj.verifySetFunction = "data";
    Object.defineProperty(obj, "0", {
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

    Object.defineProperty(obj, "0", {
        get: getFunc,
        set: setFunc
    });

    verifyEqualTo(obj, "0", getFunc());

    verifyWritable(obj, "0", "verifySetFunction1");

    verifyNotEnumerable(obj, "0");

    verifyConfigurable(obj, "0");
} finally {
    delete obj[0];
    delete obj.verifySetFunction;
    delete obj.verifySetFunction1;
}
