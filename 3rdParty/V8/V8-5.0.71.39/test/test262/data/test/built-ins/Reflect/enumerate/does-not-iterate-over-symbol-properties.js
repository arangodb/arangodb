// Copyright (C) 2015 Leonardo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.5
description: >
  Returned iterator does not iterate over symbol properties.
info: >
  26.1.5 Reflect.enumerate ( target )

  ...
  2. Return target.[[Enumerate]]().
features: [Symbol]
---*/

var iter, step;
var s = Symbol('1');

var o = {
  'a': 1,
  'b': 1
};

o[s] = 1;

iter = Reflect.enumerate(o);

step = iter.next();
assert.sameValue(step.value, 'a');
assert.sameValue(step.done, false);

step = iter.next();
assert.sameValue(step.value, 'b');
assert.sameValue(step.done, false);

step = iter.next();
assert.sameValue(step.value, undefined);
assert.sameValue(step.done, true);
