// Copyright (C) 2011 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 13.1
description: >
    for-in to acquire properties from object
includes: [arrayContains.js]
---*/
function props(x) {
  var array = [];
  for (let p in x) array.push(p);
  return array;
}

assert.sameValue(props({}).length, 0);
assert.sameValue(props({x:1}).length, 1);
assert.sameValue(props({x:1, y:2}).length, 2);
assert.sameValue(props({x:1, y:2, zoom:3}).length, 3);

assert(arrayContains(props({x:1}), ["x"]));
assert(arrayContains(props({x:1, y:2}), ["x", "y"]));
assert(arrayContains(props({x:1, y:2, zoom:3}), ["x", "y", "zoom"]));
