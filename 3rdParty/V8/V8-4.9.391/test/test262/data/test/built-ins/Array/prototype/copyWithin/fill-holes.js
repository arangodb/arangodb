// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.3
description: >
  Loop from each property, even empty holes.
---*/

var arr = [0, 1, , , 1];

arr.copyWithin(0, 1, 4);

assert.sameValue(arr.length, 5);
assert.sameValue(arr[0], 1);
assert.sameValue(arr[4], 1);
assert.sameValue(arr.hasOwnProperty(1), false);
assert.sameValue(arr.hasOwnProperty(2), false);
assert.sameValue(arr.hasOwnProperty(3), false);
