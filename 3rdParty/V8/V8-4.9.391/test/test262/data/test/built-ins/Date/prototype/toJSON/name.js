// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.37
description: >
  Date.prototype.toJSON.name is "toJSON".
info: >
  Date.prototype.toJSON ( key )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.toJSON.name, "toJSON");

verifyNotEnumerable(Date.prototype.toJSON, "name");
verifyNotWritable(Date.prototype.toJSON, "name");
verifyConfigurable(Date.prototype.toJSON, "name");
