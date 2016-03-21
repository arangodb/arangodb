// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.4.3-1-49-s
description: >
    Strict Mode - checking 'this' (FunctionExpression with a strict
    directive prologue defined within a FunctionExpression)
flags: [noStrict]
includes: [fnGlobalObject.js]
---*/

var f1 = function () {
    var f = function () {
        "use strict";
        return typeof this;
    }
    return (f()==="undefined") && (this===fnGlobalObject());
}

assert(f1(), 'f1() !== true');
