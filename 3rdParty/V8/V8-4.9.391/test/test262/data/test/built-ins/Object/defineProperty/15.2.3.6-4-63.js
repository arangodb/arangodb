// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-63
description: >
    Object.defineProperty - both desc.value and name.value are NaN
    (8.12.9 step 6)
includes: [propertyHelper.js]
---*/

var obj = {};

Object.defineProperty(obj, "foo", { value: NaN });

Object.defineProperty(obj, "foo", { value: NaN });

assert(isNaN(obj.foo));

verifyNotWritable(obj, "foo");

verifyNotEnumerable(obj, "foo");

verifyNotConfigurable(obj, "foo");
