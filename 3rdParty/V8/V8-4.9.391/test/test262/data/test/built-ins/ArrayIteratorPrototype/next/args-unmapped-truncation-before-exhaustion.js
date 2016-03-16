// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.4.4.6 S7
description: >
    Prior to being exhausted, iterators for unmapped arguments exotic objects
    should honor argument removal.
flags: [noStrict]
---*/

(function(a, b, c) {
  'use strict';
  var iterator = arguments[Symbol.iterator]();
  var result;

  iterator.next();
  iterator.next();

  arguments.length = 2;

  result = iterator.next();
  assert.sameValue(result.value, undefined, 'Exhausted result `value`');
  assert.sameValue(result.done, true, 'Exhausted result `done` flag');
}(2, 1, 3));
