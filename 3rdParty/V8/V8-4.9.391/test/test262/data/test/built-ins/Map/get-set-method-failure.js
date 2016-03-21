// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.1.1
description: >
  new Map returns abrupt from getting Map.prototype.set.
description: >
  Map ( [ iterable ] )

  ...
  7. Else,
    a. Let adder be Get(map, "add").
    b. ReturnIfAbrupt(adder).
---*/

Object.defineProperty(Map.prototype, 'set', {
  get: function() {
    throw new Test262Error();
  }
});

new Map();

assert.throws(Test262Error, function() {
  new Map([]);
});
