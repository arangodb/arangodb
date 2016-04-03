// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.4.3-1-80-s
description: >
    Strict Mode - checking 'this' (strict function declaration called
    by Function.prototype.bind(globalObject)())
flags: [noStrict]
includes: [fnGlobalObject.js]
---*/

function f() { "use strict"; return this;};

assert.sameValue(f.bind(fnGlobalObject())(), fnGlobalObject(), 'f.bind(fnGlobalObject())()');
