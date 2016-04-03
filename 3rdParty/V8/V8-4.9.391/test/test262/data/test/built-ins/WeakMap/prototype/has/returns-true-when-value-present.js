// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.4
description: >
  Returns true when value is present in the WeakMap entries list.
info: >
  WeakMap.prototype.has ( value )

  ...
  6. Repeat for each Record {[[key]], [[value]]} p that is an element of
  entries,
    a. If p.[[key]] is not empty and SameValue(p.[[key]], key) is true, return
    true.
  ...
---*/

var foo = {};
var map = new WeakMap();

map.set(foo, 1);
assert.sameValue(map.has(foo), true);
