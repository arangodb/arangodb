// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.6
description: >
  Returns `this`.
info: >
  12. Return O.
---*/

var arr = [];
var result = arr.fill(1);

assert.sameValue(result, arr);

var o = {
  length: 0
};
result = Array.prototype.fill.call(o);
assert.sameValue(result, o);
