// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.4
description: >
  Date.prototype.getFullYear.name is "getFullYear".
info: >
  Date.prototype.getFullYear ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.getFullYear.name, "getFullYear");

verifyNotEnumerable(Date.prototype.getFullYear, "name");
verifyNotWritable(Date.prototype.getFullYear, "name");
verifyConfigurable(Date.prototype.getFullYear, "name");
