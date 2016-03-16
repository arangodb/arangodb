// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.3.4.40
description: >
  Date.prototype.toLocaleTimeString.name is "toLocaleTimeString".
info: >
  Date.prototype.toLocaleTimeString ( [ reserved1 [ , reserved2 ] ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Date.prototype.toLocaleTimeString.name, "toLocaleTimeString");

verifyNotEnumerable(Date.prototype.toLocaleTimeString, "name");
verifyNotWritable(Date.prototype.toLocaleTimeString, "name");
verifyConfigurable(Date.prototype.toLocaleTimeString, "name");
