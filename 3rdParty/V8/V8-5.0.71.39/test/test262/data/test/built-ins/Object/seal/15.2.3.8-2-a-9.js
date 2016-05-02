// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.8-2-a-9
description: >
    Object.seal - 'P' is own property of a String object which
    implements its own [[GetOwnProperty]]
includes: [propertyHelper.js]
---*/

var obj = new String("abc");

obj.foo = 10;

assert(Object.isExtensible(obj));
Object.seal(obj);

verifyNotConfigurable(obj, "foo");
assert.sameValue(obj.foo, 10);
