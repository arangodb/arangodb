// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.4
description: >
  Return false when value is not present in the WeakMap entries.
info: >
  WeakMap.prototype.has ( value )

  ...
  7. Return false.

---*/

var foo = {};
var bar = {};
var map = new WeakMap();

assert.sameValue(map.has(foo), false);

map.set(foo, 1);
assert.sameValue(map.has(bar), false);

map.delete(foo);
assert.sameValue(map.has(foo), false);
