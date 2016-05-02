// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-6-4
description: >
    Allow reserved words as property names by dot operator assignment,
    accessed via indexing: new, var, catch
---*/

        var tokenCodes  = {};
        tokenCodes.new = 0;
        tokenCodes.var = 1;
        tokenCodes.catch = 2;
        var arr = [
            'new', 
            'var', 
            'catch'
         ];
         for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
