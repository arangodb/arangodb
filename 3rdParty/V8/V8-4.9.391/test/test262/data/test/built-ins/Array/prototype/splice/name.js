// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.1.3.25
description: >
  Array.prototype.splice.name is "splice".
info: >
  Array.prototype.splice (start, deleteCount , ...items )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Array.prototype.splice.name, "splice");

verifyNotEnumerable(Array.prototype.splice, "name");
verifyNotWritable(Array.prototype.splice, "name");
verifyConfigurable(Array.prototype.splice, "name");
