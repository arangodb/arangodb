// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Behavior if error is thrown when accessing `sticky` property
es6id: 21.2.5.13
info: >
    21.2.5.13 RegExp.prototype.test( S )

    [...]
    5. Let match be RegExpExec(R, string).

    21.2.5.2.2 Runtime Semantics: RegExpBuiltinExec ( R, S )

    [...]
    8. Let sticky be ToBoolean(Get(R, "sticky")).
    9. ReturnIfAbrupt(sticky).
---*/

var r = /./;

Object.defineProperty(r, 'sticky', {
  get: function() {
    throw new Test262Error();
  }
});

assert.throws(Test262Error, function() {
  r.test();
});
