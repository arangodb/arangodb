// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.23
description: >
  Date.prototype.setMilliseconds.name is "setMilliseconds".
info: >
  Date.prototype.setMilliseconds ( ms )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.setMilliseconds.name, "setMilliseconds");

verifyNotEnumerable(Date.prototype.setMilliseconds, "name");
verifyNotWritable(Date.prototype.setMilliseconds, "name");
verifyConfigurable(Date.prototype.setMilliseconds, "name");
