// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.3.11
description: >
  %TypedArray%.prototype.findIndex.name is "findIndex".
info: >
  %TypedArray%.prototype.findIndex (predicate [ , thisArg ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js, testTypedArray.js]
---*/

assert.sameValue(TypedArray.prototype.findIndex.name, "findIndex");

verifyNotEnumerable(TypedArray.prototype.findIndex, "name");
verifyNotWritable(TypedArray.prototype.findIndex, "name");
verifyConfigurable(TypedArray.prototype.findIndex, "name");
