// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.1.4.3
description: >
  ArrayBuffer.prototype.slice is not a constructor function.
info: >
  ArrayBuffer.prototype.slice ( start, end )

  17 ECMAScript Standard Built-in Objects:
    Built-in function objects that are not identified as constructors do not
    implement the [[Construct]] internal method unless otherwise specified
    in the description of a particular function.
---*/

assert.sameValue(Object.prototype.hasOwnProperty.call(ArrayBuffer.prototype.slice, "prototype"), false);

var arrayBuffer = new ArrayBuffer(8);
assert.throws(TypeError, function() { new arrayBuffer.slice(); });
