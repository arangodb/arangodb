// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.38
description: >
  Date.prototype.toLocaleDateString.name is "toLocaleDateString".
info: >
  Date.prototype.toLocaleDateString ( [ reserved1 [ , reserved2 ] ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.toLocaleDateString.name, "toLocaleDateString");

verifyNotEnumerable(Date.prototype.toLocaleDateString, "name");
verifyNotWritable(Date.prototype.toLocaleDateString, "name");
verifyConfigurable(Date.prototype.toLocaleDateString, "name");
