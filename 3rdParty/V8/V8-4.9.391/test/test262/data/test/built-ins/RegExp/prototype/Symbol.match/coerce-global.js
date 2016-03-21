// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Boolean coercion of `global` property
es6id: 21.2.5.6
info: >
    21.2.5.6 RegExp.prototype [ @@match ] ( string )

    [...]
    5. Let global be ToBoolean(Get(rx, "global")).
    [...]
features: [Symbol.match]
---*/

var r = /a/;
var result;
Object.defineProperty(r, 'global', { writable: true });

r.global = undefined;
result = r[Symbol.match]('aa');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);

r.global = null;
result = r[Symbol.match]('aa');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);

r.global = true;
result = r[Symbol.match]('aa');
assert.notSameValue(result, null);
assert.notSameValue(result.length, 1);

r.global = false;
result = r[Symbol.match]('aa');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);

r.global = NaN;
result = r[Symbol.match]('aa');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);

r.global = 0;
result = r[Symbol.match]('aa');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);

r.global = '';
result = r[Symbol.match]('aa');
assert.notSameValue(result, null);
assert.sameValue(result.length, 1);

r.global = 86;
result = r[Symbol.match]('aa');
assert.notSameValue(result, null);
assert.sameValue(result.length, 2);

r.global = Symbol.match;
result = r[Symbol.match]('aa');
assert.notSameValue(result, null);
assert.sameValue(result.length, 2);

r.global = {};
result = r[Symbol.match]('aa');
assert.notSameValue(result, null);
assert.sameValue(result.length, 2);
