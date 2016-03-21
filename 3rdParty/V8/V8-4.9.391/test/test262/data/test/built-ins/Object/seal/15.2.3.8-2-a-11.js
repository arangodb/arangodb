// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.8-2-a-11
description: >
    Object.seal - 'P' is own property of a Number object that uses
    Object's [[GetOwnProperty]]
includes: [propertyHelper.js]
---*/

var obj = new Number(-1);

obj.foo = 10;

assert(Object.isExtensible(obj));
Object.seal(obj);

verifyNotConfigurable(obj, "foo");
assert.sameValue(obj.foo, 10);
