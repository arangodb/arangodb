// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.1
description: >
    arrow functions
includes: [compareArray.js]
---*/
var fn = (a, b, ...c) => c;

assert(compareArray(fn(), []), "`compareArray(fn(), [])` returns `true`");
assert(compareArray(fn(1, 2), []), "`compareArray(fn(1, 2), [])` returns `true`");
assert(compareArray(fn(1, 2, 3), [3]),"`compareArray(fn(1, 2, 3), [3])` returns `true`");
assert(compareArray(fn(1, 2, 3, 4), [3, 4]),"`compareArray(fn(1, 2, 3, 4), [3, 4])` returns `true`");
assert(compareArray(fn(1, 2, 3, 4, 5), [3, 4, 5]),"`compareArray(fn(1, 2, 3, 4, 5), [3, 4, 5])` returns `true`");
assert(compareArray(((...args) => args)(), []),"`compareArray(((...args) => args)(), [])` returns `true`");
assert(compareArray(((...args) => args)(1,2,3), [1,2,3]),"`compareArray(((...args) => args)(1,2,3), [1,2,3])` returns `true`");
