// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.30
description: >
  Date.prototype.setUTCHours.name is "setUTCHours".
info: >
  Date.prototype.setUTCHours ( hour [ , min [ , sec [ , ms ] ] ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.setUTCHours.name, "setUTCHours");

verifyNotEnumerable(Date.prototype.setUTCHours, "name");
verifyNotWritable(Date.prototype.setUTCHours, "name");
verifyConfigurable(Date.prototype.setUTCHours, "name");
