// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-7-6
description: >
    Allow reserved words as property names by index assignment,
    accessed via indexing: continue, for, switch
---*/

        var tokenCodes = {};
        tokenCodes['continue'] = 0;
        tokenCodes['for'] = 1;
        tokenCodes['switch'] = 2;     
        var arr = [
            'continue',
            'for',
            'switch'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
