// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.3.5
description: >
  %TypedArray%.prototype.copyWithin.length is 2.
info: >
  %TypedArray%.prototype.copyWithin (target, start [, end ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, has a length
    property whose value is an integer. Unless otherwise specified, this
    value is equal to the largest number of named arguments shown in the
    subclause headings for the function description, including optional
    parameters. However, rest parameters shown using the form “...name”
    are not included in the default argument count.

    Unless otherwise specified, the length property of a built-in Function
    object has the attributes { [[Writable]]: false, [[Enumerable]]: false,
    [[Configurable]]: true }.
includes: [propertyHelper.js, testTypedArray.js]
---*/

assert.sameValue(TypedArray.prototype.copyWithin.length, 2);

verifyNotEnumerable(TypedArray.prototype.copyWithin, "length");
verifyNotWritable(TypedArray.prototype.copyWithin, "length");
verifyConfigurable(TypedArray.prototype.copyWithin, "length");
