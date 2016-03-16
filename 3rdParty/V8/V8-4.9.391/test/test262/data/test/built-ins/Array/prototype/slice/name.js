// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.1.3.22
description: >
  Array.prototype.slice.name is "slice".
info: >
  Array.prototype.slice (start, end)

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Array.prototype.slice.name, "slice");

verifyNotEnumerable(Array.prototype.slice, "name");
verifyNotWritable(Array.prototype.slice, "name");
verifyConfigurable(Array.prototype.slice, "name");
