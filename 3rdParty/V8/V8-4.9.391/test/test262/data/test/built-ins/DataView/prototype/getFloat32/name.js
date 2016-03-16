// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.2.4.5
description: >
  DataView.prototype.getFloat32.name is "getFloat32".
info: >
  DataView.prototype.getFloat32 ( byteOffset [ , littleEndian ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(DataView.prototype.getFloat32.name, "getFloat32");

verifyNotEnumerable(DataView.prototype.getFloat32, "name");
verifyNotWritable(DataView.prototype.getFloat32, "name");
verifyConfigurable(DataView.prototype.getFloat32, "name");
