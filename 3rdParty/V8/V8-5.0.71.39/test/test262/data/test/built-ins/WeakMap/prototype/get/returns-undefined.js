// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.3
description: >
  Returns undefined when key is not on the WeakMap object.
info: >
  WeakMap.prototype.get ( key )

  4. Let entries be the List that is the value of M’s [[WeakMapData]] internal
  slot.
  5. If Type(key) is not Object, return undefined.
  6. Repeat for each Record {[[key]], [[value]]} p that is an element of
  entries,
    a. If p.[[key]] is not empty and SameValue(p.[[key]], key) is true, return
    p.[[value]].
  7. Return undefined.
  ...
---*/

var map = new WeakMap();
var key = {};

assert.sameValue(
  map.get(key), undefined,
 'returns undefined if key is not on the weakmap'
);

map.set(key, 1);
map.set({}, 2);
map.delete(key);
map.set({}, 3);

assert.sameValue(
  map.get(key), undefined,
  'returns undefined if key was deleted'
);
