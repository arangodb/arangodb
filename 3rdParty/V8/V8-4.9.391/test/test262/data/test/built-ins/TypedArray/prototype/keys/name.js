// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.3.15
description: >
  %TypedArray%.prototype.keys.name is "keys".
info: >
  %TypedArray%.prototype.keys ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js, testTypedArray.js]
---*/

assert.sameValue(TypedArray.prototype.keys.name, "keys");

verifyNotEnumerable(TypedArray.prototype.keys, "name");
verifyNotWritable(TypedArray.prototype.keys, "name");
verifyConfigurable(TypedArray.prototype.keys, "name");
