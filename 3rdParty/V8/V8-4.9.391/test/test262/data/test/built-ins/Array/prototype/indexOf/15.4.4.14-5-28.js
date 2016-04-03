// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-5-28
description: >
    Array.prototype.indexOf - side effects produced by step 1 are
    visible when an exception occurs
---*/

        var stepFiveOccurs = false;
        var fromIndex = {
            valueOf: function () {
                stepFiveOccurs = true;
                return 0;
            }
        };
assert.throws(TypeError, function() {
            Array.prototype.indexOf.call(undefined, undefined, fromIndex);
});
assert.sameValue(stepFiveOccurs, false, 'stepFiveOccurs');
