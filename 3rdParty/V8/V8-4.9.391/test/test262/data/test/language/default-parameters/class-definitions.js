// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.5
description: Set default parameters on class definitions
---*/

class C {
  constructor(a = 1) {
    this.a = a;
  }

  m(x = 2) {
    return x;
  }
}

var o = new C();

assert.sameValue(o.a, 1);
assert.sameValue(o.m(), 2);
