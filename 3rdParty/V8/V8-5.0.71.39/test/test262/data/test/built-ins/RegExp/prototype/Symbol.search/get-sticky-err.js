// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Behavior if error is thrown when accessing `sticky` property
es6id: 21.2.5.9
info: >
    21.2.5.9 RegExp.prototype [ @@search ] ( string )

    [...]
    9. Let result be RegExpExec(rx, S).

    21.2.5.2.2 Runtime Semantics: RegExpBuiltinExec ( R, S )

    [...]
    8. Let sticky be ToBoolean(Get(R, "sticky")).
    9. ReturnIfAbrupt(sticky).
features: [Symbol.search]
---*/

var r = /./;
Object.defineProperty(r, 'sticky', {
  get: function() {
    throw new Test262Error();
  }
});

assert.throws(Test262Error, function() {
  r[Symbol.search]();
});
