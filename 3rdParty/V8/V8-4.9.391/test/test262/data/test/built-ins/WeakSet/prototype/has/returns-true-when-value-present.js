// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.3.4
description: >
  Returns true when value is present in the WeakSet entries list.
info: >
  WeakSet.prototype.has ( value )

  ...
  6. Repeat for each e that is an element of entries,
    a. If e is not empty and SameValue(e, value) is true, return true.
  ...
---*/

var foo = {};
var s = new WeakSet();

s.add(foo);
assert.sameValue(s.has(foo), true);
