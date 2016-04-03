// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.2
description: >
  Delete an entry.
info: >
  WeakMap.prototype.delete ( value )

  ...
  5. Let entries be the List that is the value of M’s [[WeakMapData]] internal
  slot.
  6. If Type(key) is not Object, return false.
  7. Repeat for each Record {[[key]], [[value]]} p that is an element of
  entries,
    a. If p.[[key]] is not empty and SameValue(p.[[key]], key) is true, then
      i. Set p.[[key]] to empty.
      ii. Set p.[[value]] to empty.
      iii. Return true.
  ...
---*/

var foo = {};
var map = new WeakMap();

map.set(foo, 42);

var result = map.delete(foo);

assert.sameValue(map.has(foo), false);
assert.sameValue(result, true, 'WeakMap#delete returns true');
