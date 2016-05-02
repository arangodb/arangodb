// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Errors thrown when retrieving attribute
es6id: 21.2.5.3
info: >
    21.2.5.3 get RegExp.prototype.flags

    [...]
    13. Let unicode be ToBoolean(Get(R, "unicode")).
    14. ReturnIfAbrupt(unicode).
---*/

var re = /./;

Object.defineProperty(re, 'unicode', {
  get: function() {
    throw new Test262Error();
  }
});

assert.throws(Test262Error, function() {
  re.flags;
});
