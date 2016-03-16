// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.14
description: >
  Date.prototype.getUTCFullYear.name is "getUTCFullYear".
info: >
  Date.prototype.getUTCFullYear ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.getUTCFullYear.name, "getUTCFullYear");

verifyNotEnumerable(Date.prototype.getUTCFullYear, "name");
verifyNotWritable(Date.prototype.getUTCFullYear, "name");
verifyConfigurable(Date.prototype.getUTCFullYear, "name");
