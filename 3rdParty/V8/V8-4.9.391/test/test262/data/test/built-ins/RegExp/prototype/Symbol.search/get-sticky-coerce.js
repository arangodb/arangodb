// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Boolean coercion of `sticky` property
es6id: 21.2.5.9
info: >
    21.2.5.9 RegExp.prototype [ @@search ] ( string )

    [...]
    9. Let result be RegExpExec(rx, S).

    21.2.5.2.2 Runtime Semantics: RegExpBuiltinExec ( R, S )

    [...]
    4. Let lastIndex be ToLength(Get(R,"lastIndex")).
    [...]
    8. Let sticky be ToBoolean(Get(R, "sticky")).
    [...]
features: [Symbol, Symbol.search]
---*/

var r = /a/;
Object.defineProperty(r, 'sticky', { writable: true });

r.sticky = undefined;
assert.sameValue(r[Symbol.search]('ba'), 1);

r.sticky = null;
assert.sameValue(r[Symbol.search]('ba'), 1);

r.sticky = true;
assert.sameValue(r[Symbol.search]('ba'), -1);

r.sticky = false;
assert.sameValue(r[Symbol.search]('ba'), 1);

r.sticky = NaN;
assert.sameValue(r[Symbol.search]('ba'), 1);

r.sticky = 0;
assert.sameValue(r[Symbol.search]('ba'), 1);

r.sticky = 86;
assert.sameValue(r[Symbol.search]('ba'), -1);

r.sticky = '';
assert.sameValue(r[Symbol.search]('ba'), 1);

r.sticky = Symbol();
assert.sameValue(r[Symbol.search]('ba'), -1);

r.sticky = {};
assert.sameValue(r[Symbol.search]('ba'), -1);
