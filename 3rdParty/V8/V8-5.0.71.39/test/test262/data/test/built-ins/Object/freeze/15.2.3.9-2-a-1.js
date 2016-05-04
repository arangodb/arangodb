// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.9-2-a-1
description: Object.freeze - 'P' is own data property
includes: [propertyHelper.js]
---*/

var obj = {};

obj.foo = 10; // default [[Configurable]] attribute value of foo: true

Object.freeze(obj);

verifyNotWritable(obj, "foo");
verifyNotConfigurable(obj, "foo");

assert.sameValue(obj.foo, 10);
