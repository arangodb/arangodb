// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.3
description: >
  Return false if value is not a non-null Object.
info: >
  WeakSet.prototype.delete ( value )

  4. If Type(value) is not Object, return false.
features: [Symbol]
---*/

var s = new WeakSet();

assert.sameValue(s.delete(1), false);
assert.sameValue(s.delete(''), false);
assert.sameValue(s.delete(null), false);
assert.sameValue(s.delete(undefined), false);
assert.sameValue(s.delete(true), false);
assert.sameValue(s.delete(Symbol()), false);
