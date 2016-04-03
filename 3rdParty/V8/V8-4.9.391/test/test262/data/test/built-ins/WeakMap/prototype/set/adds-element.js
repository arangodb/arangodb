// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.3.5
description: >
  Appends value as the last element of entries.
info: >
  WeakMap.prototype.set ( key, value )

  ...
  7. Let p be the Record {[[key]]: key, [[value]]: value}.
  8. Append p as the last element of entries.
  ...
---*/

var map = new WeakMap();
var foo = {};
var bar = {};
var baz = {};

map.set(foo, 1);
map.set(bar, 2);
map.set(baz, 3);

assert(map.has(foo));
assert(map.has(bar));
assert(map.has(baz));
