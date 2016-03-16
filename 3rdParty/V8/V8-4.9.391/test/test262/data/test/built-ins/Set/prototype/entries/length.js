// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.5
description: >
    Set.prototype.entries ( )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.entries.length, 0, "The value of `Set.prototype.entries.length` is `0`");

verifyNotEnumerable(Set.prototype.entries, "length");
verifyNotWritable(Set.prototype.entries, "length");
verifyConfigurable(Set.prototype.entries, "length");
