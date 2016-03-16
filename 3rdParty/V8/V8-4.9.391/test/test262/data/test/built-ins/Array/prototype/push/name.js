// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.1.3.17
description: >
  Array.prototype.push.name is "push".
info: >
  Array.prototype.push ( ...items )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Array.prototype.push.name, "push");

verifyNotEnumerable(Array.prototype.push, "name");
verifyNotWritable(Array.prototype.push, "name");
verifyConfigurable(Array.prototype.push, "name");
