// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Behavior when error thrown by accessing the `global` property
es6id: 21.2.5.6
info: >
    [...]
    5. Let global be ToBoolean(Get(rx, "global")).
    6. ReturnIfAbrupt(global).
    7. If global is false, then
       a. Return RegExpExec(rx, S).

    21.2.5.2.1 Runtime Semantics: RegExpExec ( R, S )

    [...]
    7. Return RegExpBuiltinExec(R, S).

    21.2.5.2.2 Runtime Semantics: RegExpBuiltinExec ( R, S )

    [...]
    6. Let global be ToBoolean(Get(R, "global")).
    7. ReturnIfAbrupt(global).
features: [Symbol.match]
---*/

var r = /./;
var callCount = 0;
Object.defineProperty(r, 'global', {
  get: function() {
    callCount += 1;

    if (callCount > 1) {
      throw new Test262Error();
    }
  }
});

assert.throws(Test262Error, function() {
  r[Symbol.match]('');
});
