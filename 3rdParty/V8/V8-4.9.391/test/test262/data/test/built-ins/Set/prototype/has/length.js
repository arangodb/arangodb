// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.7
description: >
    Set.prototype.has ( value )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.has.length, 1, "The value of `Set.prototype.has.length` is `1`");

verifyNotEnumerable(Set.prototype.has, "length");
verifyNotWritable(Set.prototype.has, "length");
verifyConfigurable(Set.prototype.has, "length");
