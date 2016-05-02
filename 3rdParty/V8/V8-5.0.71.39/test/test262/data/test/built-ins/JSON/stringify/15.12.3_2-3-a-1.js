// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3_2-3-a-1
description: >
    JSON.stringify converts string wrapper objects returned from
    replacer functions to literal strings.
---*/

assert.sameValue(JSON.stringify([42], function(k,v) {return v===42? new String('fortytwo'):v}), '["fortytwo"]', 'JSON.stringify([42], function(k,v) {return v===42? new String("fortytwo"):v})');
