// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.2-2-30
description: >
    Object.getPrototypeOf returns the [[Prototype]] of its parameter
    (the global object)
includes: [fnGlobalObject.js]
---*/

        var proto = Object.getPrototypeOf(fnGlobalObject());

assert.sameValue(proto.isPrototypeOf(fnGlobalObject()), true, 'proto.isPrototypeOf(fnGlobalObject())');
