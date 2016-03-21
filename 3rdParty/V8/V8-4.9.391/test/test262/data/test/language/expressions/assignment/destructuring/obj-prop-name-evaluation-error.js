// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Any error raised as a result of evaluating PropertyName should be forwarded
    to the runtime.
es6id: 12.14.5.2
---*/

var a, x;

assert.throws(TypeError, function() {
  ({ [a.b]: x } = {});
});
