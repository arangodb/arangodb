// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.6.1-5-10
description: >
    Allow reserved words as property names at object initialization,
    accessed via indexing: in, try, class
---*/

        var tokenCodes  = { 
            in: 0, 
            try: 1,
            class: 2
        };
        var arr = [
            'in', 
            'try',
            'class'
        ];
        for (var i = 0; i < arr.length; i++) {
            assert.sameValue(tokenCodes[arr[i]], i, 'tokenCodes[arr[i]]');
        }
