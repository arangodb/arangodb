// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.3.17
description: >
  get %TypedArray%.prototype.length.name is "get length".
info: >
  get %TypedArray%.prototype.length

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js, testTypedArray.js]
---*/

var desc = Object.getOwnPropertyDescriptor(TypedArray.prototype, "length");

assert.sameValue(desc.get.name, "get length");

verifyNotEnumerable(desc.get, "name");
verifyNotWritable(desc.get, "name");
verifyConfigurable(desc.get, "name");
