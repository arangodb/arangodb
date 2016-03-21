// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-310
description: >
    Object.defineProperty - 'O' is an Arguments object, 'name' is an
    index named accessor property of 'O' but not defined in
    [[ParameterMap]] of 'O', test TypeError is thrown when updating
    the [[Set]] attribute value of 'name' which is not configurable
    (10.6 [[DefineOwnProperty]] step 4)
includes: [propertyHelper.js]
---*/

(function () {
    function getFunc() {
        return 0;
    }
    Object.defineProperty(arguments, "0", {
        get: getFunc,
        set: undefined,
        enumerable: false,
        configurable: false
    });
    function setFunc(value) {
        this.setVerifyHelpProp = value;
    }
    try {
        Object.defineProperty(arguments, "0", {
            set: setFunc
        });            
        $ERROR("Expected an exception.");
    } catch (e) {
        verifyEqualTo(arguments, "0", getFunc());

        verifyNotEnumerable(arguments, "0");

        verifyNotConfigurable(arguments, "0");

        if (!(e instanceof TypeError)) {
            $ERROR("Expected TypeError, got " + e);
        }

    }
}());
