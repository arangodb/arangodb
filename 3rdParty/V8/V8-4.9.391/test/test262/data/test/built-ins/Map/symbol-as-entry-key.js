// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.4
description: >
    Symbol as Map key
features: [Symbol]
---*/
var map = new Map();
var sym = Symbol();

map.set(sym, 1);

assert.sameValue(map.size, 1, "The value of `map.size` is `1`, after executing `map.set(sym, 1)`");
assert.sameValue(map.has(sym), true, "`map.has(sym)` returns `true`");
assert.sameValue(map.get(sym), 1, "`map.get(sym)` returns `1`");
assert.sameValue(map.delete(sym), true, "`map.delete(sym)` returns `true`");
assert.sameValue(map.size, 0, "The value of `map.size` is `0`");
