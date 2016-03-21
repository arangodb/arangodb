// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-7-5
description: >
    Allow reserved words as property names by index assignment,
    accessed via indexing: finally, return, void
---*/

        var tokenCodes = {};
        tokenCodes['finally'] = 0;
        tokenCodes['return'] = 1;
        tokenCodes['void'] = 2;      
        var arr = [
            'finally',
            'return',
            'void'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
