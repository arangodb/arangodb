// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.6
description: >
    Set.prototype.forEach ( callbackfn [ , thisArg ] )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(Set.prototype.forEach.name, "forEach", "The value of `Set.prototype.forEach.name` is `'forEach'`");

verifyNotEnumerable(Set.prototype.forEach, "name");
verifyNotWritable(Set.prototype.forEach, "name");
verifyConfigurable(Set.prototype.forEach, "name");
