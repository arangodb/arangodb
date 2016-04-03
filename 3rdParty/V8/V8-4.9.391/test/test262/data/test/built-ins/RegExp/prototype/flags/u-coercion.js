// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Boolean coercion of `unicode` property
es6id: 21.2.5.3
info: >
    21.2.5.3 get RegExp.prototype.flags

    [...]
    13. Let unicode be ToBoolean(Get(R, "unicode")).
    14. ReturnIfAbrupt(unicode).
    15. If unicode is true, append "u" as the last code unit of result.
features: [Symbol]
---*/

var r = /./;
var flags;
Object.defineProperty(r, 'unicode', { writable: true });

r.unicode = undefined;
flags = r.flags;
assert.sameValue(flags.length, 0);

r.unicode = null;
flags = r.flags;
assert.sameValue(flags.length, 0);

r.unicode = NaN;
flags = r.flags;
assert.sameValue(flags.length, 0);

r.unicode = 86;
flags = r.flags;
assert.sameValue(flags.length, 1);
assert.sameValue(flags[0], 'u');

r.unicode = '';
flags = r.flags;
assert.sameValue(flags.length, 0);

r.unicode = 'string';
flags = r.flags;
assert.sameValue(flags.length, 1);
assert.sameValue(flags[0], 'u');

r.unicode = Symbol();
flags = r.flags;
assert.sameValue(flags.length, 1);
assert.sameValue(flags[0], 'u');

r.unicode = {};
flags = r.flags;
assert.sameValue(flags.length, 1);
assert.sameValue(flags[0], 'u');
