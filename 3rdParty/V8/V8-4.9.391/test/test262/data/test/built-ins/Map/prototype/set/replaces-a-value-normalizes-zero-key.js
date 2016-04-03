// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.9
description: >
  Replaces a value in the map normalizing +0 and -0.
info: >
  Map.prototype.set ( key , value )

  ...
  5. Repeat for each Record {[[key]], [[value]]} p that is an element of
  entries,
    a. If p.[[key]] is not empty and SameValueZero(p.[[key]], key) is true, then
      i. Set p.[[value]] to value.
      ii. Return M.
  ...
---*/

var map = new Map([[+0, 1]]);

map.set(-0, 42);
assert.sameValue(map.get(+0), 42, 'zero key is normalized in SameValueZero');

map = new Map([[-0, 1]]);
map.set(+0, 42);
assert.sameValue(map.get(-0), 42, 'zero key is normalized in SameValueZero');
