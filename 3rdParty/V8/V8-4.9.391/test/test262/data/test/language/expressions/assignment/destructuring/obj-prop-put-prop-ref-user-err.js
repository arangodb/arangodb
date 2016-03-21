// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Any error raised as a result of setting the value should be forwarded to
    the runtime.
es6id: 12.14.5.4
---*/

var value = { a: 23 };
var x = {
  set y(val) {
    throw new Test262Error();
  }
};

assert.throws(Test262Error, function() {
  ({ a: x.y } = value);
});
