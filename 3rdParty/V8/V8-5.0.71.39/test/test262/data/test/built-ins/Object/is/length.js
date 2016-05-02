// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.1.2.10
description: >
    Object.is ( value1, value2 )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Object.is.length, 2, "The value of `Object.is.length` is `2`");

verifyNotEnumerable(Object.is, "length");
verifyNotWritable(Object.is, "length");
verifyConfigurable(Object.is, "length");
