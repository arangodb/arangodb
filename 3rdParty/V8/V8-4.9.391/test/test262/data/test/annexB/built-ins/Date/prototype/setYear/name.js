// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: B.2.4.2
description: >
  Date.prototype.setYear.name is "setYear".
info: >
  Date.prototype.setYear ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.setYear.name, "setYear");

verifyNotEnumerable(Date.prototype.setYear, "name");
verifyNotWritable(Date.prototype.setYear, "name");
verifyConfigurable(Date.prototype.setYear, "name");
