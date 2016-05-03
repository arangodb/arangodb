// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.3
description: >
  Returns the value from the specified key
info: >
  WeakMap.prototype.get ( key )

  4. Let entries be the List that is the value of M’s [[WeakMapData]] internal
  slot.
  5. If Type(key) is not Object, return undefined.
  6. Repeat for each Record {[[key]], [[value]]} p that is an element of
  entries,
    a. If p.[[key]] is not empty and SameValue(p.[[key]], key) is true, return
    p.[[value]].
  ...
---*/

var foo = {};
var bar = {};
var baz = [];
var map = new WeakMap([[foo,0]]);

assert.sameValue(map.get(foo), 0);

map.set(bar, 1);
assert.sameValue(map.get(bar), 1);

map.set(baz, 2);
assert.sameValue(map.get(baz), 2);
