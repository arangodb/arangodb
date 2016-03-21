// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Type coercion of `sticky` property value
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
    8. Let sticky be ToBoolean(Get(R, "sticky")).
    [...]
    18. If global is true or sticky is true,
        a. Let setStatus be Set(R, "lastIndex", e, true).
features: [Symbol.match]
---*/

var r = /./;
var val;
Object.defineProperty(r, 'sticky', {
  get: function() {
    return val;
  }
});

val = false;
r[Symbol.match]('a');
assert.sameValue(r.lastIndex, 0, 'literal false');

val = '';
r[Symbol.match]('a');
assert.sameValue(r.lastIndex, 0, 'empty string');

val = 0;
r[Symbol.match]('a');
assert.sameValue(r.lastIndex, 0, 'zero');

val = null;
r[Symbol.match]('a');
assert.sameValue(r.lastIndex, 0, 'null');

val = undefined;
r[Symbol.match]('a');
assert.sameValue(r.lastIndex, 0, 'undefined');

val = true;
r[Symbol.match]('a');
assert.sameValue(r.lastIndex, 1, 'literal true');

r.lastIndex = 0;
val = 'truthy';
r[Symbol.match]('a');
assert.sameValue(r.lastIndex, 1, 'non-empty string');

r.lastIndex = 0;
val = 86;
r[Symbol.match]('a');
assert.sameValue(r.lastIndex, 1, 'nonzero number');

r.lastIndex = 0;
val = [];
r[Symbol.match]('a');
assert.sameValue(r.lastIndex, 1, 'array');

r.lastIndex = 0;
val = Symbol.match;
r[Symbol.match]('a');
assert.sameValue(r.lastIndex, 1, 'symbol');
