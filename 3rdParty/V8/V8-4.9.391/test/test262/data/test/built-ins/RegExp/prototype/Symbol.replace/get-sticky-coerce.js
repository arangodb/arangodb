// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Boolean coercion of `sticky` property
es6id: 21.2.5.8
info: >
    21.2.5.8 RegExp.prototype [ @@replace ] ( string, replaceValue )

    [...]
    13. Repeat, while done is false
        a. Let result be RegExpExec(rx, S).

    21.2.5.2.2 Runtime Semantics: RegExpBuiltinExec ( R, S )

    [...]
    8. Let sticky be ToBoolean(Get(R, "sticky")).
    9. ReturnIfAbrupt(sticky).
features: [Symbol.replace]
---*/

var r = /a/;
Object.defineProperty(r, 'sticky', { writable: true });

r.sticky = undefined;
assert.sameValue(r[Symbol.replace]('ba', 'x'), 'bx');

r.sticky = null;
assert.sameValue(r[Symbol.replace]('ba', 'x'), 'bx');

r.sticky = true;
assert.sameValue(r[Symbol.replace]('ba', 'x'), 'ba');

r.sticky = false;
assert.sameValue(r[Symbol.replace]('ba', 'x'), 'bx');

r.sticky = NaN;
assert.sameValue(r[Symbol.replace]('ba', 'x'), 'bx');

r.sticky = 0;
assert.sameValue(r[Symbol.replace]('ba', 'x'), 'bx');

r.sticky = 86;
assert.sameValue(r[Symbol.replace]('ba', 'x'), 'ba');

r.sticky = '';
assert.sameValue(r[Symbol.replace]('ba', 'x'), 'bx');

r.sticky = Symbol();
assert.sameValue(r[Symbol.replace]('ba', 'x'), 'ba');

r.sticky = {};
assert.sameValue(r[Symbol.replace]('ba', 'x'), 'ba');
