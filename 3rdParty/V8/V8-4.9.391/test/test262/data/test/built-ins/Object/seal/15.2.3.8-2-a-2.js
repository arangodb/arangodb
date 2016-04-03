// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.8-2-a-2
description: >
    Object.seal - 'P' is own data property that overrides an inherited
    data property
includes: [propertyHelper.js]
---*/

var proto = { foo: 0 };

var ConstructFun = function () { };
ConstructFun.prototype = proto;

var obj = new ConstructFun();
Object.defineProperty(obj, "foo", {
    value: 10,
    configurable: true
});

assert(Object.isExtensible(obj));
Object.seal(obj);

verifyNotConfigurable(obj, "foo");
assert.sameValue(obj.foo, 10);
