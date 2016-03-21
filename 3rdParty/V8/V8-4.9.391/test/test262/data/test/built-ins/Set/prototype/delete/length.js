// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.4
description: >
    Set.prototype.delete ( value )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.delete.length, 1, "The value of `Set.prototype.delete.length` is `1`");

verifyNotEnumerable(Set.prototype.delete, "length");
verifyNotWritable(Set.prototype.delete, "length");
verifyConfigurable(Set.prototype.delete, "length");
