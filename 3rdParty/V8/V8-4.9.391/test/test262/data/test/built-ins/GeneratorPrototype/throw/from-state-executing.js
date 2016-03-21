// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.3.3.2
description: >
    A TypeError should be thrown if the generator is resumed abruptly while
    running.
---*/

var iter;
function* g() {
  iter.throw(42);
}

iter = g();
assert.throws(TypeError, function() {
  iter.next();
});
