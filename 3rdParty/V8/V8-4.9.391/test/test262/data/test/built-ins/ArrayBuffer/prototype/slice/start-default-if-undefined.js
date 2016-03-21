// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.1.4.3
description: >
  The `start` index defaults to 0 if undefined.
info: >
  ArrayBuffer.prototype.slice ( start, end )

  ...
  6. Let relativeStart be ToInteger(start).
  7. ReturnIfAbrupt(relativeStart).
  ...
---*/

var arrayBuffer = new ArrayBuffer(8);

var start = undefined, end = 6;
var result = arrayBuffer.slice(start, end);
assert.sameValue(result.byteLength, 6);
