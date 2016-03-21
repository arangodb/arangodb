// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.10
description: >
    Set.prototype.values ( )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
    typeof Set.prototype.values,
    "function",
    "`typeof Set.prototype.values` is `'function'`"
);

verifyNotEnumerable(Set.prototype, "values");
verifyWritable(Set.prototype, "values");
verifyConfigurable(Set.prototype, "values");
