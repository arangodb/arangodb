// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.7
description: >
    Set.prototype.has ( value )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
    typeof Set.prototype.has,
    "function",
    "`typeof Set.prototype.has` is `'function'`"
);

verifyNotEnumerable(Set.prototype, "has");
verifyWritable(Set.prototype, "has");
verifyConfigurable(Set.prototype, "has");
