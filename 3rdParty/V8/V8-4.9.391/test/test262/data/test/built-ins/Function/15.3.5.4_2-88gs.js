// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.5.4_2-88gs
description: >
    Strict mode - checking access to strict function caller from
    non-strict function (non-strict function declaration called by
    strict Function.prototype.call(globalObject))
flags: [noStrict]
features: [caller]
includes: [fnGlobalObject.js]
---*/

function f() { return gNonStrict();};
(function () {"use strict"; f.call(fnGlobalObject()); })();


function gNonStrict() {
    return gNonStrict.caller;
}
