// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.3
description: >
  Returns undefined when key is not an Object.
info: >
  WeakMap.prototype.get ( key )

  ...
  4. Let entries be the List that is the value of M’s [[WeakMapData]] internal
  slot.
  5. If Type(key) is not Object, return undefined.
  ...
---*/

var map = new WeakMap();

assert.sameValue(map.get(null), undefined, 'Returns undefined if key is null');

assert.sameValue(map.get(NaN), undefined, 'Returns undefined if key is NaN');

assert.sameValue(
  map.get('foo'), undefined,
  'Returns undefined if key is a String'
);

assert.sameValue(
  map.get(1), undefined,
  'Returns undefined if key is a Number'
);

assert.sameValue(
  map.get(undefined), undefined,
  'Returns undefined if key is undefined'
);

assert.sameValue(
  map.get(Symbol()), undefined,
  'Returns undefined if key is a Symbol'
);
