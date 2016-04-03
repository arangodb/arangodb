// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-8-a-3
description: >
    JSON.stringify treats an null space argument the same as a missing
    space argument.
---*/

  var obj = {a1: {b1: [1,2,3,4], b2: {c1: 1, c2: 2}},a2: 'a2'};

assert.sameValue(JSON.stringify(obj), JSON.stringify(obj,null, null), 'JSON.stringify(obj)');
