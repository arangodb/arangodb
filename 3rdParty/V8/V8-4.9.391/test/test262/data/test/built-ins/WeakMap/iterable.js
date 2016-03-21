// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.3.1.1
description: >
  Returns the new WeakMap adding the objects from the iterable parameter.
info: >
  WeakMap ( [ iterable ] )

  ...
  9. Repeat
    k. Let status be Call(adder, map, «k.[[value]], v.[[value]]»).
    l. If status is an abrupt completion, return IteratorClose(iter, status).
includes: [compareArray.js]
---*/

var first = {};
var second = {};
var results = [];
var set = WeakMap.prototype.set;
WeakMap.prototype.set = function(key, value) {
  results.push({
    _this: this,
    key: key,
    value: value
  });
  return set.call(this, key, value);
};
var map = new WeakMap([[first, 42], [second, 43]]);

assert.sameValue(results.length, 2, 'Called WeakMap#set for each object');
assert.sameValue(results[0].key, first, 'Adds object in order - first key');
assert.sameValue(results[0].value, 42, 'Adds object in order - first value');
assert.sameValue(results[0]._this, map, 'Adds object in order - this');
assert.sameValue(results[1].key, second, 'Adds object in order - second key');
assert.sameValue(results[1].value, 43, 'Adds object in order - second value');
assert.sameValue(results[1]._this, map, 'Adds object in order - this');
