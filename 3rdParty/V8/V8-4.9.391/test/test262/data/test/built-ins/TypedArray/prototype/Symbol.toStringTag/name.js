// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.3.31
description: >
  get %TypedArray%.prototype [ @@toStringTag ].name is "get [Symbol.toStringTag]".
info: >
  get %TypedArray%.prototype [ @@toStringTag ]

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js, testTypedArray.js]
features: [Symbol.toStringTag]
---*/

var desc = Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag);

assert.sameValue(desc.get.name, "get [Symbol.toStringTag]");

verifyNotEnumerable(desc.get, "name");
verifyNotWritable(desc.get, "name");
verifyConfigurable(desc.get, "name");
