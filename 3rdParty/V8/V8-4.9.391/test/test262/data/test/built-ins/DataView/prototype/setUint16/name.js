// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.2.4.19
description: >
  DataView.prototype.setUint16.name is "setUint16".
info: >
  DataView.prototype.setUint16 ( byteOffset, value [ , littleEndian ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(DataView.prototype.setUint16.name, "setUint16");

verifyNotEnumerable(DataView.prototype.setUint16, "name");
verifyNotWritable(DataView.prototype.setUint16, "name");
verifyConfigurable(DataView.prototype.setUint16, "name");
