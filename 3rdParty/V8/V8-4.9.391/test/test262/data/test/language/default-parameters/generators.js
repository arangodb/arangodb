// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.4
description: Set default parameters in generator functions
---*/

function *g(a = 1, b = 2, c = 3) {
  var i = 0;

  while (i < 3) {
    yield [a, b, c][i];
    i++;
  }

  return 42;
}

var iter = g();

assert.sameValue(iter.next().value, 1);
assert.sameValue(iter.next().value, 2);
assert.sameValue(iter.next().value, 3);
assert.sameValue(iter.next().done, true);
