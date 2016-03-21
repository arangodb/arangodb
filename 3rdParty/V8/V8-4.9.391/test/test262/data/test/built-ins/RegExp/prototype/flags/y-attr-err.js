// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Behavior if error is thrown when retrieving `sticky` attribute
es6id: 21.2.5.3
info: >
    21.2.5.3 get RegExp.prototype.flags

    16. Let sticky be ToBoolean(Get(R, "sticky")).
    17. ReturnIfAbrupt(sticky).
---*/

var re = /./;

Object.defineProperty(re, 'sticky', {
  get: function() {
    throw new Test262Error();
  }
});

assert.throws(Test262Error, function() {
  re.flags;
});
