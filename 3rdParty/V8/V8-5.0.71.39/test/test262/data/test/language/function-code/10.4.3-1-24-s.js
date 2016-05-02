// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.4.3-1-24-s
description: >
    Strict Mode - checking 'this' (New'ed object from
    FunctionExpression includes strict directive prologue)
flags: [noStrict]
includes: [fnGlobalObject.js]
---*/

var f = function () {
    "use strict";
    return this;
}

assert.notSameValue((new f()), fnGlobalObject(), '(new f())');
assert.notSameValue(typeof (new f()), "undefined", 'typeof (new f())');
