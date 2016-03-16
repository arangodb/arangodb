// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.3.24
description: >
  %TypedArray%.prototype.some.name is "some".
info: >
  %TypedArray%.prototype.some ( callbackfn [ , thisArg ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js, testTypedArray.js]
---*/

assert.sameValue(TypedArray.prototype.some.name, "some");

verifyNotEnumerable(TypedArray.prototype.some, "name");
verifyNotWritable(TypedArray.prototype.some, "name");
verifyConfigurable(TypedArray.prototype.some, "name");
