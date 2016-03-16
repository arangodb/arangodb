// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 19.1.3.5
description: >
  Object.prototype.toLocaleString.name is "toLocaleString".
info: >
  Object.prototype.toLocaleString ( [ reserved1 [ , reserved2 ] ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Object.prototype.toLocaleString.name, "toLocaleString");

verifyNotEnumerable(Object.prototype.toLocaleString, "name");
verifyNotWritable(Object.prototype.toLocaleString, "name");
verifyConfigurable(Object.prototype.toLocaleString, "name");
