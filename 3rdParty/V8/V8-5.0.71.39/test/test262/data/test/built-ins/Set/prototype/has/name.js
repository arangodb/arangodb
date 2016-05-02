// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.7
description: >
    Set.prototype.has ( value )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.has.name, "has", "The value of `Set.prototype.has.name` is `'has'`");

verifyNotEnumerable(Set.prototype.has, "name");
verifyNotWritable(Set.prototype.has, "name");
verifyConfigurable(Set.prototype.has, "name");
