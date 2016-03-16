// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.21
description: >
  Date.prototype.setFullYear.name is "setFullYear".
info: >
  Date.prototype.setFullYear ( year [ , month [ , date ] ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.setFullYear.name, "setFullYear");

verifyNotEnumerable(Date.prototype.setFullYear, "name");
verifyNotWritable(Date.prototype.setFullYear, "name");
verifyConfigurable(Date.prototype.setFullYear, "name");
