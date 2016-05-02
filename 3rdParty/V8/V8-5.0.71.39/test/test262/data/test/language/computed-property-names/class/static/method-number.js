// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 12.2.5
description: >
    In a class, static computed property method names can be a number
includes: [compareArray.js]
---*/
class C {
  static a() { return 'A'; }
  static [1]() { return 'B'; }
  static c() { return 'C'; }
  static [2]() { return 'D'; }
}
assert.sameValue(C.a(), 'A', "`C.a()` returns `'A'`. Defined as `static a() { return 'A'; }`");
assert.sameValue(C[1](), 'B', "`C[1]()` returns `'B'`. Defined as `static [1]() { return 'B'; }`");
assert.sameValue(C.c(), 'C', "`C.c()` returns `'C'`. Defined as `static c() { return 'C'; }`");
assert.sameValue(C[2](), 'D', "`C[2]()` returns `'D'`. Defined as `static [2]() { return 'D'; }`");
assert(
  compareArray(Object.keys(C), []),
  "`compareArray(Object.keys(C), [])` returns `true`"
);
assert(
  compareArray(Object.getOwnPropertyNames(C), ['1', '2', 'length', 'prototype', 'a', 'c', 'name']),
  "`compareArray(Object.getOwnPropertyNames(C), ['1', '2', 'length', 'prototype', 'a', 'c', 'name'])` returns `true`"
);
