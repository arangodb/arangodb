// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-6-a-2
description: >
    JSON.stringify truccates non-integer numeric space arguments to
    their integer part.
---*/

  var obj = {a1: {b1: [1,2,3,4], b2: {c1: 1, c2: 2}},a2: 'a2'};

assert.sameValue(JSON.stringify(obj,null, 5.99999), JSON.stringify(obj,null, 5), 'JSON.stringify(obj,null, 5.99999)');
