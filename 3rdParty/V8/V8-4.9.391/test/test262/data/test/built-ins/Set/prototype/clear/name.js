// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.2
description: >
    Set.prototype.clear ( )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.clear.name, "clear", "The value of `Set.prototype.clear.name` is `'clear'`");

verifyNotEnumerable(Set.prototype.clear, "name");
verifyNotWritable(Set.prototype.clear, "name");
verifyConfigurable(Set.prototype.clear, "name");
