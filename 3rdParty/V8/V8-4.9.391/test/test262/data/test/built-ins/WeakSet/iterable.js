// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.4.1.1
description: >
  Returns the new WeakSet adding the objects from the iterable parameter.
info: >
  WeakSet ( [ iterable ] )

  ...
  9. Repeat
    f. Let status be Call(adder, set, «nextValue»).
    g. If status is an abrupt completion, return IteratorClose(iter, status).
includes: [compareArray.js]
---*/

var first = {};
var second = {};
var added = [];
var add = WeakSet.prototype.add;
WeakSet.prototype.add = function(value) {
  added.push(value);
  return add.call(this, value);
};
var s = new WeakSet([first, second]);

assert.sameValue(added.length, 2, 'Called WeakSet#add for each object');
assert.sameValue(added[0], first, 'Adds object in order - first');
assert.sameValue(added[1], second, 'Adds object in order - second');
