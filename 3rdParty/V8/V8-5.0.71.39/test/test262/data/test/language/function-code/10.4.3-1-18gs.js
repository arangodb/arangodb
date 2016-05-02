// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.4.3-1-18gs
description: >
    Strict - checking 'this' from a global scope (eval includes strict
    directive prologue)
flags: [noStrict]
includes: [fnGlobalObject.js]
---*/

if (eval("\"use strict\";\nthis") !== fnGlobalObject()) {
    throw "'this' had incorrect value!";
}
