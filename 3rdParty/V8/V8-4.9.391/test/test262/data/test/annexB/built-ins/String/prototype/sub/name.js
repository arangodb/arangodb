// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: B.2.3.13
description: >
  String.prototype.sub.name is "sub".
info: >
  String.prototype.sub ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(String.prototype.sub.name, "sub");

verifyNotEnumerable(String.prototype.sub, "name");
verifyNotWritable(String.prototype.sub, "name");
verifyConfigurable(String.prototype.sub, "name");
