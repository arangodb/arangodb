// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 13.6.4.13 S5.i.i
description: >
    The left-hand side may take the form of a DestructuringAssignment.
---*/

var iterationCount = 0;
var x;

for ([x] of [[0]]) {
  assert.sameValue(x, 0);
  iterationCount += 1;
}

assert.sameValue(iterationCount, 1);
