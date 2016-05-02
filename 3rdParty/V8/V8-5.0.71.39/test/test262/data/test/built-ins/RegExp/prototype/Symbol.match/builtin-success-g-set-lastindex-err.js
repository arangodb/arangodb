// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Behavior when error thrown while setting `lastIndex` after a "global" match
    success
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
    18. If global is true or sticky is true,
        a. Let setStatus be Set(R, "lastIndex", e, true).
        b. ReturnIfAbrupt(setStatus).
features: [Symbol.match]
---*/

var r = /b/;
var callCount = 0;

Object.defineProperty(r, 'lastIndex', { writable: false });
Object.defineProperty(r, 'global', {
  get: function() {
    callCount += 1;
    return callCount > 1;
  }
});

assert.throws(TypeError, function() {
  r[Symbol.match]('abc');
});
