// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-11-12
description: >
    A JSON.stringify replacer function applied to a top level scalar
    can return an Array.
---*/

assert.sameValue(JSON.stringify(42, function(k, v) { return v==42 ?[4,2]:v }), '[4,2]', 'JSON.stringify(42, function(k, v) { return v==42 ?[4,2]:v })');
