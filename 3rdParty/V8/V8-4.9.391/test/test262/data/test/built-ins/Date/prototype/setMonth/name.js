// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.25
description: >
  Date.prototype.setMonth.name is "setMonth".
info: >
  Date.prototype.setMonth ( month [ , date ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.setMonth.name, "setMonth");

verifyNotEnumerable(Date.prototype.setMonth, "name");
verifyNotWritable(Date.prototype.setMonth, "name");
verifyConfigurable(Date.prototype.setMonth, "name");
