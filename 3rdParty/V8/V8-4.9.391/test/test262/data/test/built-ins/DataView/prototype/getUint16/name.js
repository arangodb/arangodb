// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.2.4.11
description: >
  DataView.prototype.getUint16.name is "getUint16".
info: >
  DataView.prototype.getUint16 ( byteOffset [ , littleEndian ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(DataView.prototype.getUint16.name, "getUint16");

verifyNotEnumerable(DataView.prototype.getUint16, "name");
verifyNotWritable(DataView.prototype.getUint16, "name");
verifyConfigurable(DataView.prototype.getUint16, "name");
