// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.4
description: >
  Returns false if value is not an Object.
info: >
  WeakMap.prototype.has ( value )

  5. If Type(key) is not Object, return false.
features: [Symbol]
---*/

var map = new WeakMap();

assert.sameValue(map.has(1), false);
assert.sameValue(map.has(''), false);
assert.sameValue(map.has(null), false);
assert.sameValue(map.has(undefined), false);
assert.sameValue(map.has(true), false);
assert.sameValue(map.has(Symbol()), false);
