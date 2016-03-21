// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-11-15
description: >
    Applying JSON.stringify with a replacer function to a function
    returns the replacer value.
---*/

assert.sameValue(JSON.stringify(function() {}, function(k,v) {return 99}), '99', 'JSON.stringify(function() {}, function(k,v) {return 99})');
