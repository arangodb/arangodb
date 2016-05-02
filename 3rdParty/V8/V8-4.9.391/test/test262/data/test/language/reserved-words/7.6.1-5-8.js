// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-5-8
description: >
    Allow reserved words as property names at object initialization,
    accessed via indexing: this, with, default
---*/

        var tokenCodes  = {       
            this: 0,  
            with: 1, 
            default: 2
        };
        var arr = [ 
            'this', 
            'with', 
            'default'
        ]; 
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
