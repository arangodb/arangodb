// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.10
description: >
    Set.prototype.values ( )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.values.name, "values", "The value of `Set.prototype.values.name` is `'values'`");

verifyNotEnumerable(Set.prototype.values, "name");
verifyNotWritable(Set.prototype.values, "name");
verifyConfigurable(Set.prototype.values, "name");
