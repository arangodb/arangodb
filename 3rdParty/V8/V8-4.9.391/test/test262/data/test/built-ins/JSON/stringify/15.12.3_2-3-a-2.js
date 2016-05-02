// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3_2-3-a-2
description: >
    JSON.stringify converts Number wrapper objects returned from
    replacer functions to literal numbers.
---*/

assert.sameValue(JSON.stringify([42], function(k,v) {return v===42? new Number(84):v}), '[84]', 'JSON.stringify([42], function(k,v) {return v===42? new Number(84):v})');
