// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.10
description: >
  Date.prototype.getTime.name is "getTime".
info: >
  Date.prototype.getTime ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.getTime.name, "getTime");

verifyNotEnumerable(Date.prototype.getTime, "name");
verifyNotWritable(Date.prototype.getTime, "name");
verifyConfigurable(Date.prototype.getTime, "name");
