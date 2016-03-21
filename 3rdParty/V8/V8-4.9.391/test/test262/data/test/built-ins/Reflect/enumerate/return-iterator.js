// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.1.5
description: >
  Return an iterator.
info: >
  26.1.5 Reflect.enumerate ( target )

  ...
  2. Return target.[[Enumerate]]().
---*/

var iter, step;
var arr = ['a', 'b', 'c'];

iter = Reflect.enumerate(arr);

step = iter.next();
assert.sameValue(step.value, '0');
assert.sameValue(step.done, false);

step = iter.next();
assert.sameValue(step.value, '1');
assert.sameValue(step.done, false);

step = iter.next();
assert.sameValue(step.value, '2');
assert.sameValue(step.done, false);

step = iter.next();
assert.sameValue(step.value, undefined);
assert.sameValue(step.done, true);

var o = {
  'a': 42,
  'b': 43,
  'c': 44
};

Object.defineProperty(o, 'd', {
  enumerable: false,
  value: 45
});

iter = Reflect.enumerate(o);

step = iter.next();
assert.sameValue(step.value, 'a');
assert.sameValue(step.done, false);

step = iter.next();
assert.sameValue(step.value, 'b');
assert.sameValue(step.done, false);

step = iter.next();
assert.sameValue(step.value, 'c');
assert.sameValue(step.done, false);

step = iter.next();
assert.sameValue(step.value, undefined);
assert.sameValue(step.done, true);
