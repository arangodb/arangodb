// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Type coercion of `global` property value
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
features: [Symbol.match]
---*/

var r = /./;
var val, result;
Object.defineProperty(r, 'global', {
  get: function() {
    return val;
  }
});

val = false;
result = r[Symbol.match]('ab');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);
assert.sameValue(result[0], 'a');

val = '';
result = r[Symbol.match]('ab');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);
assert.sameValue(result[0], 'a');

val = 0;
result = r[Symbol.match]('ab');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);
assert.sameValue(result[0], 'a');

val = null;
result = r[Symbol.match]('ab');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);
assert.sameValue(result[0], 'a');

val = undefined;
result = r[Symbol.match]('ab');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);
assert.sameValue(result[0], 'a');

val = true;
result = r[Symbol.match]('ab');
assert.notSameValue(result, null);
assert.sameValue(result.length, 2);
assert.sameValue(result[0], 'a');
assert.sameValue(result[1], 'b');

val = 'truthy';
result = r[Symbol.match]('ab');
assert.notSameValue(result, null);
assert.sameValue(result.length, 2);
assert.sameValue(result[0], 'a');
assert.sameValue(result[1], 'b');

val = 86;
result = r[Symbol.match]('ab');
assert.notSameValue(result, null);
assert.sameValue(result.length, 2);
assert.sameValue(result[0], 'a');
assert.sameValue(result[1], 'b');

val = [];
result = r[Symbol.match]('ab');
assert.notSameValue(result, null);
assert.sameValue(result.length, 2);
assert.sameValue(result[0], 'a');
assert.sameValue(result[1], 'b');

val = Symbol.match;
result = r[Symbol.match]('ab');
assert.notSameValue(result, null);
assert.sameValue(result.length, 2);
assert.sameValue(result[0], 'a');
assert.sameValue(result[1], 'b');
