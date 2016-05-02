// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.4
description: >
    Symbol as Set entry
features: [Set]
---*/
var set = new Set();
var sym = Symbol();

set.add(sym);

assert.sameValue(set.size, 1, "The value of `set.size` is `1`, after executing `set.add(sym)`");
assert.sameValue(set.has(sym), true, "`set.has(sym)` returns `true`");
assert.sameValue(set.delete(sym), true, "`set.delete(sym)` returns `true`");
assert.sameValue(set.size, 0, "The value of `set.size` is `0`");
