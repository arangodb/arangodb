// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.3
description: Set default parameters on method definitions
---*/

var o = {
  m(a = 1, b = 2) {
    return a + b;
  }
};

assert.sameValue(o.m(), 3);
