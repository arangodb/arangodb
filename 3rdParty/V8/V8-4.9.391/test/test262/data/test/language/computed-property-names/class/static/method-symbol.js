// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 12.2.5
description: >
    In a class, static computed property method names can be a symbol
includes: [compareArray.js]
---*/
var sym1 = Symbol();
var sym2 = Symbol();
class C {
  static a() { return 'A'; }
  static [sym1]() { return 'B'; }
  static c() { return 'C'; }
  static [sym2]() { return 'D'; }
}
assert.sameValue(C.a(), 'A', "`C.a()` returns `'A'`. Defined as `static a() { return 'A'; }`");
assert.sameValue(C[sym1](), 'B', "`C[sym1]()` returns `'B'`. Defined as `static [sym1]() { return 'B'; }`");
assert.sameValue(C.c(), 'C', "`C.c()` returns `'C'`. Defined as `static c() { return 'C'; }`");
assert.sameValue(C[sym2](), 'D', "`C[sym2]()` returns `'D'`. Defined as `static [sym2]() { return 'D'; }`");
assert(
  compareArray(Object.keys(C), []),
  "`compareArray(Object.keys(C), [])` returns `true`"
);
assert(
  compareArray(Object.getOwnPropertyNames(C), ['length', 'prototype', 'a', 'c', 'name']),
  "`compareArray(Object.getOwnPropertyNames(C), ['length', 'prototype', 'a', 'c', 'name'])` returns `true`"
);
assert(
  compareArray(Object.getOwnPropertySymbols(C), [sym1, sym2]),
  "`compareArray(Object.getOwnPropertySymbols(C), [sym1, sym2])` returns `true`"
);
