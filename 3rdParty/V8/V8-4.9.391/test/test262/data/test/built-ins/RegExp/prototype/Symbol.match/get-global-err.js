// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Behavior when error is thrown during retrieval of `global` property
es6id: 21.2.5.6
info: >
    5. Let global be ToBoolean(Get(rx, "global")).
    6. ReturnIfAbrupt(global).
features: [Symbol.match]
---*/

var obj = {
  get global() {
    throw new Test262Error();
  }
};

assert.throws(Test262Error, function() {
  RegExp.prototype[Symbol.match].call(obj);
});
