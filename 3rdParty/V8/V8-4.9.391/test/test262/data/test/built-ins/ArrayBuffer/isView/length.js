// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.1.3.1
description: >
  ArrayBuffer.isView.length is 1.
info: >
  ArrayBuffer.isView ( arg )

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
includes: [propertyHelper.js]
---*/

assert.sameValue(ArrayBuffer.isView.length, 1);

verifyNotEnumerable(ArrayBuffer.isView, "length");
verifyNotWritable(ArrayBuffer.isView, "length");
verifyConfigurable(ArrayBuffer.isView, "length");
