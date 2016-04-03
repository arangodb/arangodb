// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.2
description: >
    Set.prototype.clear ( )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.clear.length, 0, "The value of `Set.prototype.clear.length` is `0`");

verifyNotEnumerable(Set.prototype.clear, "length");
verifyNotWritable(Set.prototype.clear, "length");
verifyConfigurable(Set.prototype.clear, "length");
