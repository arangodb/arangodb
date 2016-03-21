// Copyright 2012 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 9.2.1_8_c_ii
description: Tests that values other than strings are not accepted as locales.
author: Norbert Lindenberg
includes: [testIntl.js]
---*/

var notStringOrObject = [undefined, null, true, false, 0, 5, -5, NaN];

testWithIntlConstructors(function (Constructor) {
    notStringOrObject.forEach(function (value) {
        var error;
        try {
            var supported = Constructor.supportedLocalesOf([value]);
        } catch (e) {
            error = e;
        }
        if (error === undefined) {
            $ERROR("" + value + " as locale was not rejected.");
        } else if (error.name !== "TypeError") {
            $ERROR("" + value + " as locale was rejected with wrong error " + error.name + ".");
        }
    });
    
    return true;
});
