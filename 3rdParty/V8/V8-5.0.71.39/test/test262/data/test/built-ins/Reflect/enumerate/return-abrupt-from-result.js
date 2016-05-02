// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.5
description: >
  Return abrupt result.
info: >
  26.1.5 Reflect.enumerate ( target )

  ...
  2. Return target.[[Enumerate]]().
features: [Proxy]
---*/

var o = {};
var p = new Proxy(o, {
  enumerate: function() {
    throw new Test262Error();
  }
});

assert.throws(Test262Error, function() {
  Reflect.enumerate(p);
});
