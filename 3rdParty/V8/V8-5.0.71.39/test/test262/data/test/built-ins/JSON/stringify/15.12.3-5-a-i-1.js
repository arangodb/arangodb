// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-5-a-i-1
description: >
    JSON.stringify converts Number wrapper object space aruguments to
    Number values
---*/

  var obj = {a1: {b1: [1,2,3,4], b2: {c1: 1, c2: 2}},a2: 'a2'};

assert.sameValue(JSON.stringify(obj,null, new Number(5)), JSON.stringify(obj,null, 5), 'JSON.stringify(obj,null, new Number(5))');
