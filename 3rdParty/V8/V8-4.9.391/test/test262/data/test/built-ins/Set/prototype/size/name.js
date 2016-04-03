// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.3.9
description: >
    get Set.prototype.size

    17 ECMAScript Standard Built-in Objects

    Functions that are specified as get or set accessor functions of built-in
    properties have "get " or "set " prepended to the property name string.

includes: [propertyHelper.js]
---*/

var descriptor = Object.getOwnPropertyDescriptor(Set.prototype, "size");


assert.sameValue(descriptor.get.name, "get size", "The value of `descriptor.get.name` is `'get size'`");

verifyNotEnumerable(descriptor.get, "name");
verifyNotWritable(descriptor.get, "name");
verifyConfigurable(descriptor.get, "name");
