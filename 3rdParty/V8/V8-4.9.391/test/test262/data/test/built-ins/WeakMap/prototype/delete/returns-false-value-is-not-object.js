// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.2
description: >
  Return false if value is not an Object.
info: >
  WeakMap.prototype.delete ( value )

  5. If Type(key) is not Object, return false.
features: [Symbol]
---*/

var map = new WeakMap();

assert.sameValue(map.delete(1), false);
assert.sameValue(map.delete(''), false);
assert.sameValue(map.delete(NaN), false);
assert.sameValue(map.delete(null), false);
assert.sameValue(map.delete(undefined), false);
assert.sameValue(map.delete(true), false);
assert.sameValue(map.delete(Symbol()), false);
