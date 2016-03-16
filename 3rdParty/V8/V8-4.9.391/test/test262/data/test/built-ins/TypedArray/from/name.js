// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.2.1
description: >
  %TypedArray%.from.name is "from".
info: >
  %TypedArray%.from ( source [ , mapfn [ , thisArg ] ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js, testTypedArray.js]
---*/

assert.sameValue(TypedArray.from.name, "from");

verifyNotEnumerable(TypedArray.from, "name");
verifyNotWritable(TypedArray.from, "name");
verifyConfigurable(TypedArray.from, "name");
