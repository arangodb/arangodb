// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 19.4.3.2
description: >
    String coercion operations on Symbols
---*/

assert.throws(TypeError, function() {
  new String(Symbol('66'));
});

assert.throws(TypeError, function() {
  Symbol('66') + '';
});


