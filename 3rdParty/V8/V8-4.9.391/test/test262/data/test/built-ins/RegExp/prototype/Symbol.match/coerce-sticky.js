// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Boolean coercion of `sticky` property
es6id: 21.2.5.6
info: >
    21.2.5.6 RegExp.prototype [ @@match ] ( string )

    [...]
    5. Let global be ToBoolean(Get(rx, "global")).
    6. ReturnIfAbrupt(global).
    7. If global is false, then
       a. Return RegExpExec(rx, S).

    21.2.5.2.2 Runtime Semantics: RegExpBuiltinExec ( R, S )

    [...]
    8. Let sticky be ToBoolean(Get(R, "sticky")).
features: [Symbol.match]
---*/

var r = /a/;
Object.defineProperty(r, 'sticky', { writable: true });

r.sticky = undefined;
assert.notSameValue(r[Symbol.match]('ba'), null);

r.sticky = null;
assert.notSameValue(r[Symbol.match]('ba'), null);

r.sticky = true;
assert.sameValue(r[Symbol.match]('ba'), null);

r.sticky = false;
assert.notSameValue(r[Symbol.match]('ba'), null);

r.sticky = NaN;
assert.notSameValue(r[Symbol.match]('ba'), null);

r.sticky = 0;
assert.notSameValue(r[Symbol.match]('ba'), null);

r.sticky = '';
assert.notSameValue(r[Symbol.match]('ba'), null);

r.sticky = 86;
assert.sameValue(r[Symbol.match]('ba'), null);

r.sticky = Symbol.match;
assert.sameValue(r[Symbol.match]('ba'), null);

r.sticky = {};
assert.sameValue(r[Symbol.match]('ba'), null);
