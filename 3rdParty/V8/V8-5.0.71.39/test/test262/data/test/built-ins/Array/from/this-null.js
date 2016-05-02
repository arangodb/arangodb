// Copyright 2015 Leonardo Balter. All rights reserved.
// This code is governed by the license found in the LICENSE file.
/*---
es6id: 22.1.2.1
description: Does not throw if this is null
---*/

var result = Array.from.call(null, []);

assert(result instanceof Array, 'Does not throw if this is null');
assert.sameValue(result.length, 0, 'result.length');
