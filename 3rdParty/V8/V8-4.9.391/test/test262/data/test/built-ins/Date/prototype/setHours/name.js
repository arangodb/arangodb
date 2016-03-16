// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.22
description: >
  Date.prototype.setHours.name is "setHours".
info: >
  Date.prototype.setHours ( hour [ , min [ , sec [ , ms ] ] ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.setHours.name, "setHours");

verifyNotEnumerable(Date.prototype.setHours, "name");
verifyNotWritable(Date.prototype.setHours, "name");
verifyConfigurable(Date.prototype.setHours, "name");
