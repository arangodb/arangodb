// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.1_S2
description: >
  Symbol may not be used as a WeakSet entry
features: [WeakSet]
---*/
var weakset = new WeakSet();
var sym = Symbol();

assert.throws(TypeError, function() {
  weakset.add(sym);
});


