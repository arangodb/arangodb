// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.1.2.1
description: >
  The `length` parameter is converted to a number value.
info: >
  ArrayBuffer( length )

  ...
  2. Let numberLength be ToNumber(length).
  ...
---*/

var log = "";
var lengthValue = {
  valueOf: function() {
    log += "ok";
    return 10;
  }
};

var arrayBuffer = new ArrayBuffer(lengthValue);

assert.sameValue(log, "ok");
assert.sameValue(arrayBuffer.byteLength, 10);
