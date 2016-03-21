// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.4
description: >
  Returns false if value is not a non-null Object.
info: >
  WeakSet.prototype.has ( value )

  5. If Type(value) is not Object, return false.
features: [Symbol]
---*/

var s = new WeakSet();

assert.sameValue(s.has(1), false);
assert.sameValue(s.has(''), false);
assert.sameValue(s.has(null), false);
assert.sameValue(s.has(undefined), false);
assert.sameValue(s.has(true), false);
assert.sameValue(s.has(Symbol()), false);
