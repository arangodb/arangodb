// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.5
description: >
    Set.prototype.entries ( )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
    typeof Set.prototype.entries,
    "function",
    "`typeof Set.prototype.entries` is `'function'`"
);

verifyNotEnumerable(Set.prototype, "entries");
verifyWritable(Set.prototype, "entries");
verifyConfigurable(Set.prototype, "entries");
