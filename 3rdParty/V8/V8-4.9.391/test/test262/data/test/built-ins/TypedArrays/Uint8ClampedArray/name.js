// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.2.5
description: >
  Uint8ClampedArray.name is "Uint8ClampedArray".
info: >
  Each TypedArray constructor has a name property whose value is the
  String value of the constructor name specified for it in Table 49.

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Uint8ClampedArray.name, "Uint8ClampedArray");

verifyNotEnumerable(Uint8ClampedArray, "name");
verifyNotWritable(Uint8ClampedArray, "name");
verifyConfigurable(Uint8ClampedArray, "name");
