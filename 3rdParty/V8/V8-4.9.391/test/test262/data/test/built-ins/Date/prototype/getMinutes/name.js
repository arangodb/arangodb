// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.7
description: >
  Date.prototype.getMinutes.name is "getMinutes".
info: >
  Date.prototype.getMinutes ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.getMinutes.name, "getMinutes");

verifyNotEnumerable(Date.prototype.getMinutes, "name");
verifyNotWritable(Date.prototype.getMinutes, "name");
verifyConfigurable(Date.prototype.getMinutes, "name");
