// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.1
description: >
    Set.prototype.add ( value )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
    typeof Set.prototype.add,
    "function",
    "`typeof Set.prototype.add` is `'function'`"
);

verifyNotEnumerable(Set.prototype, "add");
verifyWritable(Set.prototype, "add");
verifyConfigurable(Set.prototype, "add");
