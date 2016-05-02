// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.6
description: >
    Set.prototype.forEach ( callbackfn [ , thisArg ] )

    The length property of the forEach method is 1.

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.forEach.length, 1, "The value of `Set.prototype.forEach.length` is `1`");

verifyNotEnumerable(Set.prototype.forEach, "length");
verifyNotWritable(Set.prototype.forEach, "length");
verifyConfigurable(Set.prototype.forEach, "length");
