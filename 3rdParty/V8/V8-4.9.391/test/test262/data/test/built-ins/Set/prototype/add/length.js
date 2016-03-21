// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.1
description: >
    Set.prototype.add ( value )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.add.length, 1, "The value of `Set.prototype.add.length` is `1`");

verifyNotEnumerable(Set.prototype.add, "length");
verifyNotWritable(Set.prototype.add, "length");
verifyConfigurable(Set.prototype.add, "length");
