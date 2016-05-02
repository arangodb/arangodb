// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-6-8
description: >
    Allow reserved words as property names by dot operator assignment,
    accessed via indexing: this, with, default
---*/

        var tokenCodes  = {};
        tokenCodes.this = 0; 
        tokenCodes.with = 1; 
        tokenCodes.default = 2;
        var arr = [
            'this', 
            'with', 
            'default'
         ];
         for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
