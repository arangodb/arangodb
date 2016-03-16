// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.1.3.21
description: >
  Array.prototype.shift.name is "shift".
info: >
  Array.prototype.shift ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Array.prototype.shift.name, "shift");

verifyNotEnumerable(Array.prototype.shift, "name");
verifyNotWritable(Array.prototype.shift, "name");
verifyConfigurable(Array.prototype.shift, "name");
