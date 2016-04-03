// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Boolean coercion of `global` property
es6id: 21.2.5.8
info: >
    21.2.5.6 RegExp.prototype [ @@replace ] ( string )

    [...]
    8. Let global be ToBoolean(Get(rx, "global")).
    [...]
features: [Symbol.replace]
---*/

var r = /a/;
Object.defineProperty(r, 'global', { writable: true });

r.global = undefined;
assert.sameValue(r[Symbol.replace]('aa', 'b'), 'ba');

r.global = null;
assert.sameValue(r[Symbol.replace]('aa', 'b'), 'ba');

r.global = false;
assert.sameValue(r[Symbol.replace]('aa', 'b'), 'ba');

r.global = NaN;
assert.sameValue(r[Symbol.replace]('aa', 'b'), 'ba');

r.global = 0;
assert.sameValue(r[Symbol.replace]('aa', 'b'), 'ba');

r.global = '';
assert.sameValue(r[Symbol.replace]('aa', 'b'), 'ba');

r.global = true;
assert.sameValue(r[Symbol.replace]('aa', 'b'), 'bb');

r.global = 86;
assert.sameValue(r[Symbol.replace]('aa', 'b'), 'bb');

r.global = Symbol.replace;
assert.sameValue(r[Symbol.replace]('aa', 'b'), 'bb');

r.global = {};
assert.sameValue(r[Symbol.replace]('aa', 'b'), 'bb');
