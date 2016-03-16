// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.11
description: >
  Date.prototype.getTimezoneOffset.name is "getTimezoneOffset".
info: >
  Date.prototype.getTimezoneOffset ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.getTimezoneOffset.name, "getTimezoneOffset");

verifyNotEnumerable(Date.prototype.getTimezoneOffset, "name");
verifyNotWritable(Date.prototype.getTimezoneOffset, "name");
verifyConfigurable(Date.prototype.getTimezoneOffset, "name");
