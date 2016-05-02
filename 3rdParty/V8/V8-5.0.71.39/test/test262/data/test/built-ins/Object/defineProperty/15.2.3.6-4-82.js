// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-82
description: >
    Object.defineProperty - desc.configurable and name.configurable
    are boolean negation of each other (8.12.9 step 6)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "foo", { configurable: true });

Object.defineProperty(obj, "foo", { configurable: false });
verifyEqualTo(obj, "foo", undefined);

verifyNotWritable(obj, "foo");

verifyNotEnumerable(obj, "foo");

verifyNotConfigurable(obj, "foo");
